// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading.Tasks;
using Horde.Build.Logs.Data;
using Horde.Build.Utilities;
using Microsoft.Extensions.Logging;
using StackExchange.Redis;

namespace Horde.Build.Logs.Storage
{
	using LogId = ObjectId<ILogFile>;

	/// <summary>
	/// Redis log file storage
	/// </summary>
	public sealed class RedisLogStorage : ILogStorage
	{
		/// <summary>
		/// The Redis database
		/// </summary>
		readonly IDatabase _redisDb;

		/// <summary>
		/// Logging device
		/// </summary>
		readonly ILogger _logger;

		/// <summary>
		/// Inner storage layer
		/// </summary>
		readonly ILogStorage _inner;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="redisDb">Redis database</param>
		/// <param name="logger">Logger instance</param>
		/// <param name="inner">Inner storage layer</param>
		public RedisLogStorage(IDatabase redisDb, ILogger logger, ILogStorage inner)
		{
			_redisDb = redisDb;
			_logger = logger;
			_inner = inner;
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_inner.Dispose();
		}

		/// <summary>
		/// Gets the key for a log file's index
		/// </summary>
		/// <param name="logId">The log file id</param>
		/// <param name="length">Length of the file covered by the index</param>
		/// <returns>The index key</returns>
		static string IndexKey(LogId logId, long length)
		{
			return $"log-{logId}-index-{length}";
		}

		/// <summary>
		/// Gets the key for a chunk's data
		/// </summary>
		/// <param name="logId">The log file id</param>
		/// <param name="offset">Offset of the chunk within the log file</param>
		/// <returns>The chunk key</returns>
		static string ChunkKey(LogId logId, long offset)
		{
			return $"log-{logId}-chunk-{offset}";
		}

		/// <summary>
		/// Adds an index to the cache
		/// </summary>
		/// <param name="key">Key for the item to store</param>
		/// <param name="value">Value to be stored</param>
		/// <returns>Async task</returns>
		async Task AddAsync(string key, ReadOnlyMemory<byte> value)
		{
			try
			{
				await _redisDb.StringSetAsync(key, value, expiry: TimeSpan.FromHours(1.0));
				_logger.LogDebug("Added key {Key} to Redis cache ({Size} bytes)", key, value.Length);
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Error writing Redis key {Key}", key);
			}
		}

		/// <summary>
		/// Adds an index to the cache
		/// </summary>
		/// <param name="key">Key for the item to store</param>
		/// <param name="deserialize">Delegate to deserialize the item</param>
		/// <returns>Async task</returns>
		async Task<T?> GetAsync<T>(string key, Func<ReadOnlyMemory<byte>, T> deserialize) where T : class
		{
			try
			{
				RedisValue value = await _redisDb.StringGetAsync(key);
				if (value.IsNullOrEmpty)
				{
					_logger.LogDebug("Redis cache miss for {Key}", key);
					return null;
				}
				else
				{
					_logger.LogDebug("Redis cache hit for {Key}", key);
					return deserialize(value);
				}
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Error reading Redis key {Key}", key);
				return null;
			}
		}

		/// <inheritdoc/>
		public async Task<LogIndexData?> ReadIndexAsync(LogId logId, long length)
		{
			string key = IndexKey(logId, length);

			LogIndexData? indexData = await GetAsync(key, memory => LogIndexData.FromMemory(memory));
			if(indexData == null)
			{
				indexData = await _inner.ReadIndexAsync(logId, length);
				if(indexData != null)
				{
					await AddAsync(key, indexData.ToByteArray());
				}
			}
			return indexData;
		}

		/// <inheritdoc/>
		public async Task WriteIndexAsync(LogId logId, long length, LogIndexData index)
		{
			await _inner.WriteIndexAsync(logId, length, index);
		}

		/// <inheritdoc/>
		public async Task<LogChunkData?> ReadChunkAsync(LogId logId, long offset, int lineIndex)
		{
			string key = ChunkKey(logId, offset);

			LogChunkData? chunkData = await GetAsync(key, memory => LogChunkData.FromMemory(memory, offset, lineIndex));
			if (chunkData == null)
			{
				chunkData = await _inner.ReadChunkAsync(logId, offset, lineIndex);
				if (chunkData != null)
				{
					await AddAsync(key, chunkData.ToByteArray(_logger));
				}
			}
			return chunkData;
		}

		/// <inheritdoc/>
		public async Task WriteChunkAsync(LogId logId, long offset, LogChunkData chunkData)
		{
			await _inner.WriteChunkAsync(logId, offset, chunkData);
		}
	}
}
