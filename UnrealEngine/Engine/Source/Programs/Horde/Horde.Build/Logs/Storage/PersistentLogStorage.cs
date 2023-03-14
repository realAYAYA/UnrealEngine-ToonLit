// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Build.Logs.Data;
using Horde.Build.Storage;
using Horde.Build.Utilities;
using Microsoft.Extensions.Logging;

namespace Horde.Build.Logs.Storage
{
	using LogId = ObjectId<ILogFile>;

	/// <summary>
	/// Bulk storage for log file data
	/// </summary>
	class PersistentLogStorage : ILogStorage
	{
		/// <summary>
		/// The bulk storage provider to use
		/// </summary>
		readonly IStorageBackend _storageProvider;

		/// <summary>
		/// Log provider
		/// </summary>
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="storageProvider">The storage provider</param>
		/// <param name="logger">Logging provider</param>
		public PersistentLogStorage(IStorageBackend<PersistentLogStorage> storageProvider, ILogger<PersistentLogStorage> logger)
		{
			_storageProvider = storageProvider;
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

			string path = $"{logId}/index_{length}";
			ReadOnlyMemory<byte>? data = await _storageProvider.ReadBytesAsync(path);
			if (data == null)
			{
				return null;
			}
			return LogIndexData.FromMemory(data.Value);
		}

		/// <inheritdoc/>
		public Task WriteIndexAsync(LogId logId, long length, LogIndexData indexData)
		{
			_logger.LogDebug("Writing log {LogId} index length {Length} to persistent storage", logId, length);

			string path = $"{logId}/index_{length}";
			ReadOnlyMemory<byte> data = indexData.ToByteArray();
			return _storageProvider.WriteBytesAsync(path, data);
		}

		/// <inheritdoc/>
		public async Task<LogChunkData?> ReadChunkAsync(LogId logId, long offset, int lineIndex)
		{
			_logger.LogDebug("Reading log {LogId} chunk offset {Offset} from persistent storage", logId, offset);

			string path = $"{logId}/offset_{offset}";
			ReadOnlyMemory<byte>? data = await _storageProvider.ReadBytesAsync(path);
			if(data == null)
			{
				return null;
			}

			MemoryReader reader = new MemoryReader(data.Value);
			LogChunkData chunkData = reader.ReadLogChunkData(offset, lineIndex);

			if (reader.Memory.Length > 0)
			{
				throw new Exception($"Serialization of persistent chunk {path} is not at expected offset ({reader.Memory.Length} bytes remaining)");
			}

			return chunkData;
		}

		/// <inheritdoc/>
		public Task WriteChunkAsync(LogId logId, long offset, LogChunkData chunkData)
		{
			_logger.LogDebug("Writing log {LogId} chunk offset {Offset} to persistent storage", logId, offset);

			string path = $"{logId}/offset_{offset}";
			byte[] data = new byte[chunkData.GetSerializedSize(_logger)];
			MemoryWriter writer = new MemoryWriter(data);
			writer.WriteLogChunkData(chunkData, _logger);
			writer.CheckEmpty();

			return _storageProvider.WriteBytesAsync(path, data);
		}
	}
}
