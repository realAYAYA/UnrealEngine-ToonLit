// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading.Tasks;
using EpicGames.Horde.Logs;
using Horde.Server.Logs.Data;
using Microsoft.Extensions.Caching.Memory;

namespace Horde.Server.Logs.Storage
{
	/// <summary>
	/// In-memory cache for chunk and index data
	/// </summary>
	class LocalLogStorage : ILogStorage
	{
		/// <summary>
		/// The memory cache instance
		/// </summary>
		readonly MemoryCache _cache;

		/// <summary>
		/// The inner storage
		/// </summary>
		readonly ILogStorage _inner;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="numItems">Maximum size for the memory cache</param>
		/// <param name="inner">The inner storage provider</param>
		public LocalLogStorage(int numItems, ILogStorage inner)
		{
			MemoryCacheOptions options = new MemoryCacheOptions();
			options.SizeLimit = numItems;

			_cache = new MemoryCache(options);
			_inner = inner;
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_cache.Dispose();
			_inner.Dispose();
		}

		/// <summary>
		/// Gets the cache key for a particular index
		/// </summary>
		/// <param name="logId">The log file to retrieve an index for</param>
		/// <param name="length">Length of the file covered by the index</param>
		/// <returns>Cache key for the index</returns>
		static string IndexKey(LogId logId, long length)
		{
			return $"{logId}/index-{length}";
		}

		/// <summary>
		/// Gets the cache key for a particular chunk
		/// </summary>
		/// <param name="logId">The log file to retrieve an index for</param>
		/// <param name="offset">The chunk offset</param>
		/// <returns>Cache key for the chunk</returns>
		static string ChunkKey(LogId logId, long offset)
		{
			return $"{logId}/chunk-{offset}";
		}

		/// <summary>
		/// Adds an entry to the cache
		/// </summary>
		/// <param name="key">The cache key</param>
		/// <param name="value">Value to store</param>
		void AddEntry(string key, object? value)
		{
			using (ICacheEntry entry = _cache.CreateEntry(key))
			{
				if (value == null)
				{
					entry.SetAbsoluteExpiration(TimeSpan.FromSeconds(30.0));
					entry.SetSize(0);
				}
				else
				{
					entry.SetSlidingExpiration(TimeSpan.FromHours(4));
					entry.SetSize(1);
				}
				entry.SetValue(value);
			}
		}

		/// <summary>
		/// Reads a value from the cache, or from the inner storage provider, adding the result to the cache
		/// </summary>
		/// <typeparam name="T">Type of object to read</typeparam>
		/// <param name="key">The cache key</param>
		/// <param name="readInner">Delegate to read from the inner storage provider</param>
		/// <returns>The retrieved value</returns>
		async Task<T?> ReadValueAsync<T>(string key, Func<Task<T?>> readInner) where T : class
		{
			T? value;
			if (!_cache.TryGetValue(key, out value))
			{
				try
				{
					value = await readInner();
				}
				finally
				{
					AddEntry(key, value);
				}
			}
			return value;
		}

		/// <inheritdoc/>
		public Task<LogIndexData?> ReadIndexAsync(LogId logId, long length)
		{
			return ReadValueAsync(IndexKey(logId, length), () => _inner.ReadIndexAsync(logId, length));
		}

		/// <inheritdoc/>
		public Task<LogChunkData?> ReadChunkAsync(LogId logId, long offset, int lineIndex)
		{
			return ReadValueAsync(ChunkKey(logId, offset), () => _inner.ReadChunkAsync(logId, offset, lineIndex));
		}
	}
}
