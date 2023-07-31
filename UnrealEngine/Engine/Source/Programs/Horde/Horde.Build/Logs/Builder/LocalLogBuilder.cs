// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;
using Horde.Build.Logs.Data;
using Horde.Build.Utilities;

namespace Horde.Build.Logs.Builder
{
	using LogId = ObjectId<ILogFile>;

	/// <summary>
	/// In-memory implementation of a log write buffer
	/// </summary>
	class LocalLogBuilder : ILogBuilder
	{
		/// <summary>
		/// Stores information about a sub-chunk which is still being written to
		/// </summary>
		class PendingSubChunk
		{
			/// <summary>
			/// The type of data stored in this sub chunk
			/// </summary>
			public LogType Type { get; }

			/// <summary>
			/// Data for the sub-chunk
			/// </summary>
			public MemoryStream Stream { get; } = new MemoryStream(4096);

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="type">Type of data stored in this subchunk</param>
			public PendingSubChunk(LogType type)
			{
				Type = type;
			}

			/// <summary>
			/// Converts this pending sub-chunk to a concrete LogSubChunkData object
			/// </summary>
			/// <param name="offset">Offset of this sub-chunk within the file</param>
			/// <param name="lineIndex">The line index</param>
			/// <returns>New sub-chunk data object</returns>
			public LogSubChunkData ToSubChunkData(long offset, int lineIndex)
			{
				return new LogSubChunkData(Type, offset, lineIndex, new ReadOnlyLogText(Stream.ToArray()));
			}
		}

		/// <summary>
		/// Stores information about a pending log chunk
		/// </summary>
		class PendingChunk
		{
			/// <summary>
			/// Time that this chunk was created
			/// </summary>
			public DateTime CreateTimeUtc { get; set; } = DateTime.UtcNow;

			/// <summary>
			/// Array of sub chunks for this chunk
			/// </summary>
			public List<PendingSubChunk> _subChunks = new List<PendingSubChunk>();

			/// <summary>
			/// The next sub-chunk
			/// </summary>
			public PendingSubChunk _nextSubChunk;

			/// <summary>
			/// Total length of this chunk
			/// </summary>
			public long _length;

			/// <summary>
			/// Whether the chunk is complete
			/// </summary>
			public bool _complete;

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="type">Type of data to store in this file</param>
			public PendingChunk(LogType type)
			{
				_nextSubChunk = new PendingSubChunk(type);
			}

			/// <summary>
			/// Complete the current sub-chunk
			/// </summary>
			public void CompleteSubChunk()
			{
				if (_nextSubChunk.Stream.Length > 0)
				{
					_subChunks.Add(_nextSubChunk);
					_nextSubChunk = new PendingSubChunk(_nextSubChunk.Type);
				}
			}
		}

		/// <summary>
		/// Current log chunk state
		/// </summary>
		readonly ConcurrentDictionary<(LogId, long), PendingChunk> _pendingChunks = new ConcurrentDictionary<(LogId, long), PendingChunk>();

		/// <inheritdoc/>
		public bool FlushOnShutdown => true;

		/// <inheritdoc/>
		public Task<bool> AppendAsync(LogId logId, long chunkOffset, long writeOffset, int writeLineIndex, int writeLineCount, ReadOnlyMemory<byte> data, LogType type)
		{
			PendingChunk? pendingChunk;
			while (!_pendingChunks.TryGetValue((logId, chunkOffset), out pendingChunk))
			{
				if(writeOffset != chunkOffset)
				{
					return Task.FromResult(false);
				}

				pendingChunk = new PendingChunk(type);

				if (_pendingChunks.TryAdd((logId, chunkOffset), pendingChunk))
				{
					break;
				}
			}

			lock (pendingChunk)
			{
				if (!pendingChunk._complete && chunkOffset + pendingChunk._length == writeOffset)
				{
					pendingChunk._nextSubChunk.Stream.Write(data.Span);
					pendingChunk._length += data.Span.Length;
					return Task.FromResult(true);
				}
			}
			return Task.FromResult(false);
		}

		/// <inheritdoc/>
		public Task CompleteSubChunkAsync(LogId logId, long offset)
		{
			PendingChunk? pendingChunk;
			if (_pendingChunks.TryGetValue((logId, offset), out pendingChunk))
			{
				lock (pendingChunk)
				{
					pendingChunk.CompleteSubChunk();
				}
			}
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public Task CompleteChunkAsync(LogId logId, long offset)
		{
			PendingChunk? pendingChunk;
			if (_pendingChunks.TryGetValue((logId, offset), out pendingChunk))
			{
				lock (pendingChunk)
				{
					if (!pendingChunk._complete)
					{
						pendingChunk._complete = true;
						pendingChunk.CompleteSubChunk();
					}
				}
			}
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public Task RemoveChunkAsync(LogId logId, long offset)
		{
			_pendingChunks.TryRemove((logId, offset), out _);
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public Task<LogChunkData?> GetChunkAsync(LogId logId, long offset, int lineIndex)
		{
			PendingChunk? pendingChunk;
			if (_pendingChunks.TryGetValue((logId, offset), out pendingChunk))
			{
				long subChunkOffset = offset;
				int subChunkLineIndex = lineIndex;

				List<LogSubChunkData> subChunks = new List<LogSubChunkData>();
				foreach(PendingSubChunk pendingSubChunk in pendingChunk._subChunks)
				{
					LogSubChunkData subChunk = pendingSubChunk.ToSubChunkData(subChunkOffset, subChunkLineIndex);
					subChunkOffset += subChunk.Length;
					subChunkLineIndex += subChunk.LineCount;
					subChunks.Add(subChunk);
				}

				PendingSubChunk nextSubChunk = pendingChunk._nextSubChunk;
				if (nextSubChunk.Stream.Length > 0)
				{
					LogSubChunkData subChunkData = nextSubChunk.ToSubChunkData(subChunkOffset, subChunkLineIndex);
					subChunks.Add(subChunkData);
				}

				return Task.FromResult<LogChunkData?>(new LogChunkData(offset, lineIndex, subChunks));
			}

			return Task.FromResult<LogChunkData?>(null);
		}

		/// <inheritdoc/>
		public Task<List<(LogId, long)>> TouchChunksAsync(TimeSpan minAge)
		{
			DateTime utcNow = DateTime.UtcNow;

			List<(LogId, long)> chunks = new List<(LogId, long)>();
			foreach (KeyValuePair<(LogId, long), PendingChunk> pendingChunk in _pendingChunks.ToArray())
			{
				lock (pendingChunk.Value)
				{
					if (pendingChunk.Value.CreateTimeUtc < utcNow - minAge)
					{
						chunks.Add(pendingChunk.Key);
						pendingChunk.Value.CreateTimeUtc = utcNow;
					}
				}
			}
			return Task.FromResult(chunks);
		}
	}
}
