// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Logs;
using Horde.Server.Logs.Data;

namespace Horde.Server.Logs.Storage
{
	/// <summary>
	/// Storage layer which caches pending read tasks, to avoid fetching the same item more than once
	/// </summary>
	class SequencedLogStorage : ILogStorage
	{
		/// <summary>
		/// Inner storage implementation
		/// </summary>
		readonly ILogStorage _inner;

		/// <summary>
		/// Pending index reads
		/// </summary>
		readonly Dictionary<(LogId, long), Task<LogIndexData?>> _indexReadTasks = new Dictionary<(LogId, long), Task<LogIndexData?>>();

		/// <summary>
		/// Pending chunk reads
		/// </summary>
		readonly Dictionary<(LogId, long), Task<LogChunkData?>> _chunkReadTasks = new Dictionary<(LogId, long), Task<LogChunkData?>>();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="inner">The inner storage provider</param>
		public SequencedLogStorage(ILogStorage inner)
		{
			_inner = inner;
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_inner.Dispose();
		}

		/// <inheritdoc/>
		public Task<LogIndexData?> ReadIndexAsync(LogId logId, long length)
		{
			Task<LogIndexData?>? task;
			lock (_indexReadTasks)
			{
				if (!_indexReadTasks.TryGetValue((logId, length), out task))
				{
					task = InnerReadIndexAsync(logId, length);
					_indexReadTasks.Add((logId, length), task);
				}
			}
			return task;
		}

		/// <inheritdoc/>
		public Task<LogChunkData?> ReadChunkAsync(LogId logId, long offset, int lineIndex)
		{
			Task<LogChunkData?>? task;
			lock (_chunkReadTasks)
			{
				if (!_chunkReadTasks.TryGetValue((logId, offset), out task))
				{
					task = InnerReadChunkAsync(logId, offset, lineIndex);
					_chunkReadTasks[(logId, offset)] = task;
				}
			}
			return task;
		}

		/// <summary>
		/// Wrapper for reading an index from the inner storage provider
		/// </summary>
		/// <param name="logId">The log file to read the index for</param>
		/// <param name="length">Length of the file that's indexed</param>
		/// <returns>The index data</returns>
		async Task<LogIndexData?> InnerReadIndexAsync(LogId logId, long length)
		{
			await Task.Yield();

			LogIndexData? indexData = await _inner.ReadIndexAsync(logId, length);
			lock (_indexReadTasks)
			{
				_indexReadTasks.Remove((logId, length));
			}
			return indexData;
		}

		/// <summary>
		/// Wrapper for reading a chunk from the inner storage provider
		/// </summary>
		/// <param name="logId">The log file to read the index for</param>
		/// <param name="offset">Offset of the chunk to read</param>
		/// <param name="lineIndex">Index of the first line in this chunk</param>
		/// <returns>The index data</returns>
		async Task<LogChunkData?> InnerReadChunkAsync(LogId logId, long offset, int lineIndex)
		{
			await Task.Yield();

			LogChunkData? chunkData = await _inner.ReadChunkAsync(logId, offset, lineIndex);
			lock (_chunkReadTasks)
			{
				_chunkReadTasks.Remove((logId, offset));
			}
			return chunkData;
		}
	}
}
