// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Storage;
using Horde.Server.Logs.Data;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Logs.Storage
{
	/// <summary>
	/// Bulk storage for log file data
	/// </summary>
	class PersistentLogStorage : ILogStorage
	{
		readonly IObjectStore _objectStore;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="objectStore">The object store</param>
		/// <param name="logger">Logging provider</param>
		public PersistentLogStorage(IObjectStore<PersistentLogStorage> objectStore, ILogger<PersistentLogStorage> logger)
		{
			_objectStore = objectStore;
			_logger = logger;
		}

		/// <inheritdoc/>
		public void Dispose()
		{
		}

		/// <inheritdoc/>
		public async Task<LogIndexData?> ReadIndexAsync(LogId logId, long length)
		{
			_logger.LogDebug("Reading log {LogId} index length {Length} from persistent storage", logId, length);

			ObjectKey key = new ObjectKey($"{logId}/index_{length}");
			IReadOnlyMemoryOwner<byte> data = await _objectStore.ReadAsync(key);
			return LogIndexData.FromMemory(data.Memory.ToArray());
		}

		/// <inheritdoc/>
		public async Task<LogChunkData?> ReadChunkAsync(LogId logId, long offset, int lineIndex)
		{
			_logger.LogDebug("Reading log {LogId} chunk offset {Offset} from persistent storage", logId, offset);

			ObjectKey key = new ObjectKey($"{logId}/offset_{offset}");
			IReadOnlyMemoryOwner<byte> data = await _objectStore.ReadAsync(key);

			MemoryReader reader = new MemoryReader(data.Memory.ToArray());
			LogChunkData chunkData = reader.ReadLogChunkData(offset, lineIndex);

			if (reader.RemainingMemory.Length > 0)
			{
				throw new Exception($"Serialization of persistent chunk {key} is not at expected offset ({reader.RemainingMemory.Length} bytes remaining)");
			}

			return chunkData;
		}
	}
}
