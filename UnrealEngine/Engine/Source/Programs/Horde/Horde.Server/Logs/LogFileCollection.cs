// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Sessions;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Storage;
using Horde.Server.Server;
using Horde.Server.Storage;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Server.Logs
{
	/// <summary>
	/// Wrapper around the jobs collection in a mongo DB
	/// </summary>
	public class LogFileCollection : ILogFileCollection
	{
		class LogChunkDocument : ILogChunk
		{
			public long Offset { get; set; }
			public int Length { get; set; }
			public int LineIndex { get; set; }

			[BsonIgnoreIfNull]
			public string? Server { get; set; }

			[BsonConstructor]
			public LogChunkDocument()
			{
			}

			public LogChunkDocument(LogChunkDocument other)
			{
				Offset = other.Offset;
				Length = other.Length;
				LineIndex = other.LineIndex;
				Server = other.Server;
			}

			public LogChunkDocument Clone()
			{
				return (LogChunkDocument)MemberwiseClone();
			}
		}

		class LogFileDocument : ILogFile
		{
			[BsonRequired, BsonId]
			public LogId Id { get; set; }

			[BsonRequired]
			public JobId JobId { get; set; }

			[BsonIgnoreIfNull]
			public LeaseId? LeaseId { get; set; }

			[BsonIgnoreIfNull]
			public SessionId? SessionId { get; set; }

			public LogType Type { get; set; }
			public bool UseNewStorageBackend { get; set; }

			[BsonIgnoreIfNull]
			public int? MaxLineIndex { get; set; }

			[BsonIgnoreIfNull]
			public long? IndexLength { get; set; }

			public List<LogChunkDocument> Chunks { get; set; } = new List<LogChunkDocument>();

			public int LineCount { get; set; }

			public NamespaceId NamespaceId { get; set; } = Namespace.Logs;
			public RefName RefName { get; set; }

			[BsonIgnoreIfDefault]
			public bool Complete { get; set; }

			[BsonRequired]
			public int UpdateIndex { get; set; }

			IReadOnlyList<ILogChunk> ILogFile.Chunks => Chunks;

			[BsonConstructor]
			private LogFileDocument()
			{
			}

			public LogFileDocument(JobId jobId, LeaseId? leaseId, SessionId? sessionId, LogType type, LogId? logId, NamespaceId namespaceId)
			{
				Id = logId ?? LogIdUtils.GenerateNewId();
				JobId = jobId;
				LeaseId = leaseId;
				SessionId = sessionId;
				Type = type;
				UseNewStorageBackend = true;
				MaxLineIndex = 0;
				NamespaceId = namespaceId;
				RefName = new RefName(Id.ToString());
			}

			public LogFileDocument Clone()
			{
				LogFileDocument document = (LogFileDocument)MemberwiseClone();
				document.Chunks = document.Chunks.ConvertAll(x => x.Clone());
				return document;
			}
		}

		/// <summary>
		/// The jobs collection
		/// </summary>
		readonly IMongoCollection<LogFileDocument> _logFiles;

		/// <summary>
		/// Hostname for the current server
		/// </summary>
		readonly string _hostName;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service singleton</param>
		public LogFileCollection(MongoService mongoService)
		{
			_logFiles = mongoService.GetCollection<LogFileDocument>("LogFiles");
			_hostName = Dns.GetHostName();
		}

		/// <inheritdoc/>
		public async Task<ILogFile> CreateLogFileAsync(JobId jobId, LeaseId? leaseId, SessionId? sessionId, LogType type, LogId? logId, CancellationToken cancellationToken)
		{
			LogFileDocument newLogFile = new LogFileDocument(jobId, leaseId, sessionId, type, logId, Namespace.Logs);
			await _logFiles.InsertOneAsync(newLogFile, null, cancellationToken);
			return newLogFile;
		}

		/// <inheritdoc/>
		public async Task<ILogFile> UpdateLineCountAsync(ILogFile logFileInterface, int lineCount, bool complete, CancellationToken cancellationToken)
		{
			FilterDefinition<LogFileDocument> filter = Builders<LogFileDocument>.Filter.Eq(x => x.Id, logFileInterface.Id);
			UpdateDefinition<LogFileDocument> update = Builders<LogFileDocument>.Update.Set(x => x.LineCount, lineCount).Set(x => x.Complete, complete).Inc(x => x.UpdateIndex, 1);
			return await _logFiles.FindOneAndUpdateAsync(filter, update, new FindOneAndUpdateOptions<LogFileDocument, LogFileDocument> { ReturnDocument = ReturnDocument.After }, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<ILogFile?> TryAddChunkAsync(ILogFile logFileInterface, long offset, int lineIndex, CancellationToken cancellationToken)
		{
			LogFileDocument logFile = ((LogFileDocument)logFileInterface).Clone();

			int chunkIdx = logFile.Chunks.GetChunkForOffset(offset) + 1;

			LogChunkDocument chunk = new LogChunkDocument();
			chunk.Offset = offset;
			chunk.LineIndex = lineIndex;
			chunk.Server = _hostName;
			logFile.Chunks.Insert(chunkIdx, chunk);

			UpdateDefinition<LogFileDocument> update = Builders<LogFileDocument>.Update.Set(x => x.Chunks, logFile.Chunks);
			if (chunkIdx == logFile.Chunks.Count - 1)
			{
				logFile.MaxLineIndex = null;
				update = update.Unset(x => x.MaxLineIndex);
			}

			if (!await TryUpdateLogFileAsync(logFile, update, cancellationToken))
			{
				return null;
			}

			return logFile;
		}

		/// <inheritdoc/>
		public async Task<ILogFile?> TryCompleteChunksAsync(ILogFile logFileInterface, IEnumerable<CompleteLogChunkUpdate> chunkUpdates, CancellationToken cancellationToken)
		{
			LogFileDocument logFile = ((LogFileDocument)logFileInterface).Clone();

			// Update the length of any complete chunks
			UpdateDefinitionBuilder<LogFileDocument> updateBuilder = Builders<LogFileDocument>.Update;
			List<UpdateDefinition<LogFileDocument>> updates = new List<UpdateDefinition<LogFileDocument>>();
			foreach (CompleteLogChunkUpdate chunkUpdate in chunkUpdates)
			{
				LogChunkDocument chunk = logFile.Chunks[chunkUpdate.Index];
				chunk.Length = chunkUpdate.Length;
				updates.Add(updateBuilder.Set(x => x.Chunks[chunkUpdate.Index].Length, chunkUpdate.Length));

				if (chunkUpdate.Index == logFile.Chunks.Count - 1)
				{
					logFile.MaxLineIndex = chunk.LineIndex + chunkUpdate.LineCount;
					updates.Add(updateBuilder.Set(x => x.MaxLineIndex, logFile.MaxLineIndex));
				}
			}

			// Try to apply the updates
			if (updates.Count > 0 && !await TryUpdateLogFileAsync(logFile, updateBuilder.Combine(updates), cancellationToken))
			{
				return null;
			}

			return logFile;
		}

		/// <inheritdoc/>
		public async Task<ILogFile?> TryUpdateIndexAsync(ILogFile logFileInterface, long newIndexLength, CancellationToken cancellationToken)
		{
			LogFileDocument logFile = ((LogFileDocument)logFileInterface).Clone();

			UpdateDefinition<LogFileDocument> update = Builders<LogFileDocument>.Update.Set(x => x.IndexLength, newIndexLength);
			if (!await TryUpdateLogFileAsync(logFile, update, cancellationToken))
			{
				return null;
			}

			logFile.IndexLength = newIndexLength;
			return logFile;
		}

		/// <inheritdoc/>
		private async Task<bool> TryUpdateLogFileAsync(LogFileDocument current, UpdateDefinition<LogFileDocument> update, CancellationToken cancellationToken)
		{
			int prevUpdateIndex = current.UpdateIndex;
			current.UpdateIndex++;
			UpdateResult result = await _logFiles.UpdateOneAsync<LogFileDocument>(x => x.Id == current.Id && x.UpdateIndex == prevUpdateIndex, update.Set(x => x.UpdateIndex, current.UpdateIndex), cancellationToken: cancellationToken);
			return result.ModifiedCount == 1;
		}

		/// <inheritdoc/>
		public async Task<ILogFile?> GetLogFileAsync(LogId logFileId, CancellationToken cancellationToken)
		{
			LogFileDocument logFile = await _logFiles.Find<LogFileDocument>(x => x.Id == logFileId).FirstOrDefaultAsync(cancellationToken);
			return logFile;
		}

		/// <inheritdoc/>
		public async Task<List<ILogFile>> GetLogFilesAsync(int? index = null, int? count = null, CancellationToken cancellationToken = default)
		{
			IFindFluent<LogFileDocument, LogFileDocument> query = _logFiles.Find(FilterDefinition<LogFileDocument>.Empty);
			if (index != null)
			{
				query = query.Skip(index.Value);
			}
			if (count != null)
			{
				query = query.Limit(count.Value);
			}

			List<LogFileDocument> results = await query.ToListAsync(cancellationToken);
			return results.ConvertAll<ILogFile>(x => x);
		}
	}
}
