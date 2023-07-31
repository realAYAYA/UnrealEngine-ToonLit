// Copyright Epic Games, Inc. All Rights Reserved.

using StackExchange.Redis;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace EpicGames.Redis
{
	/// <summary>
	/// Represents a typed Redis list with a given key
	/// </summary>
	/// <typeparam name="TElement">The type of element stored in the list</typeparam>
	public readonly struct RedisList<TElement>
	{
		internal readonly RedisConnectionPool? ConnectionPool = null;
		internal readonly IDatabaseAsync? Database = null;

		/// <summary>
		/// The key for the list
		/// </summary>
		public readonly RedisKey Key { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="database">Explicitly set Redis database (overrides use of any connection pool)</param>
		/// <param name="key">Redis key this type is using</param>
		public RedisList(IDatabaseAsync database, RedisKey key)
		{
			Database = database;
			Key = key;
		}
		
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="connectionPool">Connection pool for Redis databases</param>
		/// <param name="key">Redis key this type is using</param>
		public RedisList(RedisConnectionPool connectionPool, RedisKey key)
		{
			ConnectionPool = connectionPool;
			Key = key;
		}
		
		/// <summary>
		/// Get the Redis database in use
		/// </summary>
		/// <returns>A connection pool or explicitly set Redis database</returns>
		/// <exception cref="InvalidOperationException">If neither are set</exception>
		public IDatabaseAsync GetDatabase()
		{
			if (Database != null) return Database;
			if (ConnectionPool != null) return ConnectionPool.GetDatabase();
			throw new InvalidOperationException($"Neither {nameof(Database)} or {nameof(ConnectionPool)} has been set!");
		}

		/// <inheritdoc cref="IDatabaseAsync.ListGetByIndexAsync(RedisKey, Int64, CommandFlags)"/>
		public async Task<TElement> GetByIndexAsync(long index, CommandFlags flags = CommandFlags.None)
		{
			RedisValue value = await GetDatabase().ListGetByIndexAsync(Key, index, flags);
			return value.IsNull? default! : RedisSerializer.Deserialize<TElement>(value)!;
		}

		/// <inheritdoc cref="IDatabaseAsync.ListInsertAfterAsync(RedisKey, RedisValue, RedisValue, CommandFlags)"/>
		public Task<long> InsertAfterAsync(TElement pivot, TElement item, CommandFlags flags = CommandFlags.None)
		{
			RedisValue pivotValue = RedisSerializer.Serialize(pivot);
			RedisValue itemValue = RedisSerializer.Serialize(item);
			return GetDatabase().ListInsertAfterAsync(Key, pivotValue, itemValue, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.ListInsertBeforeAsync(RedisKey, RedisValue, RedisValue, CommandFlags)"/>
		public Task<long> InsertBeforeAsync(TElement pivot, TElement item, CommandFlags flags = CommandFlags.None)
		{
			RedisValue pivotValue = RedisSerializer.Serialize(pivot);
			RedisValue itemValue = RedisSerializer.Serialize(item);
			return GetDatabase().ListInsertBeforeAsync(Key, pivotValue, itemValue, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.ListLeftPopAsync(RedisKey, CommandFlags)"/>
		public async Task<TElement> LeftPopAsync(CommandFlags flags = CommandFlags.None)
		{
			RedisValue value = await GetDatabase().ListLeftPopAsync(Key, flags);
			return value.IsNull ? default! : RedisSerializer.Deserialize<TElement>(value)!;
		}

		/// <inheritdoc cref="IDatabaseAsync.ListLeftPushAsync(RedisKey, RedisValue, When, CommandFlags)"/>
		public Task<long> LeftPushAsync(TElement item, When when = When.Always, CommandFlags flags = CommandFlags.None)
		{
			return GetDatabase().ListLeftPushAsync(Key, RedisSerializer.Serialize(item), when, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.ListLeftPushAsync(RedisKey, RedisValue[], When, CommandFlags)"/>
		public Task<long> LeftPushAsync(IEnumerable<TElement> items, When when = When.Always, CommandFlags flags = CommandFlags.None)
		{
			RedisValue[] values = items.Select(x => RedisSerializer.Serialize(x)).ToArray();
			return GetDatabase().ListLeftPushAsync(Key, values, when, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.ListLengthAsync(RedisKey, CommandFlags)"/>
		public Task<long> LengthAsync()
		{
			return GetDatabase().ListLengthAsync(Key);
		}

		/// <inheritdoc cref="IDatabaseAsync.ListRangeAsync(RedisKey, Int64, Int64, CommandFlags)"/>
		public async Task<TElement[]> RangeAsync(long start = 0, long stop = -1, CommandFlags flags = CommandFlags.None)
		{
			RedisValue[] values = await GetDatabase().ListRangeAsync(Key, start, stop, flags);
			return Array.ConvertAll(values, (Converter<RedisValue, TElement>)(x => (TElement)RedisSerializer.Deserialize<TElement>(x)!));
		}

		/// <inheritdoc cref="IDatabaseAsync.ListRemoveAsync(RedisKey, RedisValue, Int64, CommandFlags)"/>
		public Task<long> RemoveAsync(TElement item, long count = 0L, CommandFlags flags = CommandFlags.None)
		{
			RedisValue value = RedisSerializer.Serialize(item);
			return GetDatabase().ListRemoveAsync(Key, value, count, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.ListRightPopAsync(RedisKey, CommandFlags)"/>
		public async Task<TElement> RightPopAsync(CommandFlags flags = CommandFlags.None)
		{
			RedisValue value = await GetDatabase().ListRightPopAsync(Key, flags);
			return value.IsNull? default! : RedisSerializer.Deserialize<TElement>(value)!;
		}

		/// <inheritdoc cref="IDatabaseAsync.ListRightPushAsync(RedisKey, RedisValue, When, CommandFlags)"/>
		public Task<long> RightPushAsync(TElement item, When when = When.Always, CommandFlags flags = CommandFlags.None)
		{
			return GetDatabase().ListRightPushAsync(Key, RedisSerializer.Serialize(item), when, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.ListRightPushAsync(RedisKey, RedisValue[], When, CommandFlags)"/>
		public Task<long> RightPushAsync(IEnumerable<TElement> items, When when = When.Always, CommandFlags flags = CommandFlags.None)
		{
			RedisValue[] values = items.Select(x => RedisSerializer.Serialize(x)).ToArray();
			return GetDatabase().ListRightPushAsync(Key, values, when, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.ListSetByIndexAsync(RedisKey, Int64, RedisValue, CommandFlags)"/>
		public Task SetByIndexAsync(long index, TElement item, CommandFlags flags = CommandFlags.None)
		{
			RedisValue value = RedisSerializer.Serialize(item);
			return GetDatabase().ListSetByIndexAsync(Key, index, value, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.ListTrimAsync(RedisKey, Int64, Int64, CommandFlags)"/>
		public Task TrimAsync(long start, long stop, CommandFlags flags = CommandFlags.None)
		{
			return GetDatabase().ListTrimAsync(Key, start, stop, flags);
		}
	}

	/// <summary>
	/// Extension methods for sets
	/// </summary>
	public static class RedisListExtensions
	{
		/// <summary>
		/// Creates a version of this list which modifies a transaction rather than the direct DB
		/// </summary>
		public static RedisList<TElement> With<TElement>(this ITransaction transaction, RedisList<TElement> set)
		{
			return new RedisList<TElement>(transaction, set.Key);
		}
	}
}
