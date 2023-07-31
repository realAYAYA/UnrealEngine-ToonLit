// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Threading.Tasks;
using Horde.Build.Agents.Sessions;
using Horde.Build.Jobs;
using Horde.Build.Server;
using Horde.Build.Utilities;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Build.Logs
{
	using JobId = ObjectId<IJob>;
	using LogId = ObjectId<ILogFile>;
	using SessionId = ObjectId<ISession>;

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

			public SessionId? SessionId { get; set; }
			public LogType Type { get; set; }

			[BsonIgnoreIfNull]
			public int? MaxLineIndex { get; set; }

			[BsonIgnoreIfNull]
			public long? IndexLength { get; set; }

			public List<LogChunkDocument> Chunks { get; set; } = new List<LogChunkDocument>();

			[BsonRequired]
			public int UpdateIndex { get; set; }

			IReadOnlyList<ILogChunk> ILogFile.Chunks => Chunks;

			[BsonConstructor]
			private LogFileDocument()
			{
			}

			public LogFileDocument(JobId jobId, SessionId? sessionId, LogType type)
			{
				Id = LogId.GenerateNewId();
				JobId = jobId;
				SessionId = sessionId;
				Type = type;
				MaxLineIndex = 0;
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
		public async Task<ILogFile> CreateLogFileAsync(JobId jobId, SessionId? sessionId, LogType type)
		{
			LogFileDocument newLogFile = new LogFileDocument(jobId, sessionId, type);
			await _logFiles.InsertOneAsync(newLogFile);
			return newLogFile;
		}

		/// <inheritdoc/>
		public async Task<ILogFile?> TryAddChunkAsync(ILogFile logFileInterface, long offset, int lineIndex)
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

			if (!await TryUpdateLogFileAsync(logFile, update))
			{
				return null;
			}

			return logFile;
		}

		/// <inheritdoc/>
		public async Task<ILogFile?> TryCompleteChunksAsync(ILogFile logFileInterface, IEnumerable<CompleteLogChunkUpdate> chunkUpdates)
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
			if (updates.Count > 0 && !await TryUpdateLogFileAsync(logFile, updateBuilder.Combine(updates)))
			{
				return null;
			}

			return logFile;
		}

		/// <inheritdoc/>
		public async Task<ILogFile?> TryUpdateIndexAsync(ILogFile logFileInterface, long newIndexLength)
		{
			LogFileDocument logFile = ((LogFileDocument)logFileInterface).Clone();

			UpdateDefinition<LogFileDocument> update = Builders<LogFileDocument>.Update.Set(x => x.IndexLength, newIndexLength);
			if (!await TryUpdateLogFileAsync(logFile, update))
			{
				return null;
			}

			logFile.IndexLength = newIndexLength;
			return logFile;
		}

		/// <inheritdoc/>
		private async Task<bool> TryUpdateLogFileAsync(LogFileDocument current, UpdateDefinition<LogFileDocument> update)
		{
			int prevUpdateIndex = current.UpdateIndex;
			current.UpdateIndex++;
			UpdateResult result = await _logFiles.UpdateOneAsync<LogFileDocument>(x => x.Id == current.Id && x.UpdateIndex == prevUpdateIndex, update.Set(x => x.UpdateIndex, current.UpdateIndex));
			return result.ModifiedCount == 1;
		}

		/// <inheritdoc/>
		public async Task<ILogFile?> GetLogFileAsync(LogId logFileId)
		{
			LogFileDocument logFile = await _logFiles.Find<LogFileDocument>(x => x.Id == logFileId).FirstOrDefaultAsync();
			return logFile;
		}

		/// <inheritdoc/>
		public async Task<List<ILogFile>> GetLogFilesAsync(int? index = null, int? count = null)
		{
			IFindFluent<LogFileDocument, LogFileDocument> query = _logFiles.Find(FilterDefinition<LogFileDocument>.Empty);
			if(index != null)
			{
				query = query.Skip(index.Value);
			}
			if(count != null)
			{
				query = query.Limit(count.Value);
			}

			List<LogFileDocument> results = await query.ToListAsync();
			return results.ConvertAll<ILogFile>(x => x);
		}
	}
}
