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
	public readonly struct RedisSet<TElement>
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
		public RedisSet(IDatabaseAsync database, RedisKey key)
		{
			Database = database;
			Key = key;
		}
		
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="connectionPool">Connection pool for Redis databases</param>
		/// <param name="key">Redis key this type is using</param>
		public RedisSet(RedisConnectionPool connectionPool, RedisKey key)
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

		/// <inheritdoc cref="IDatabaseAsync.SetAddAsync(RedisKey, RedisValue, CommandFlags)"/>
		public Task<bool> AddAsync(TElement item, CommandFlags flags = CommandFlags.None)
		{
			return GetDatabase().SetAddAsync(Key, RedisSerializer.Serialize(item), flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.SetAddAsync(RedisKey, RedisValue[], CommandFlags)"/>
		public Task<long> AddAsync(IEnumerable<TElement> items, CommandFlags flags = CommandFlags.None)
		{
			RedisValue[] values = items.Select(x => RedisSerializer.Serialize(x)).ToArray();
			return GetDatabase().SetAddAsync(Key, values, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.SetContainsAsync(RedisKey, RedisValue, CommandFlags)"/>
		public Task<bool> ContainsAsync(TElement item, CommandFlags flags = CommandFlags.None)
		{
			RedisValue value = RedisSerializer.Serialize(item);
			return GetDatabase().SetContainsAsync(Key, value, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.SetLengthAsync(RedisKey, CommandFlags)"/>
		public Task<long> LengthAsync(CommandFlags flags = CommandFlags.None)
		{
			return GetDatabase().SetLengthAsync(Key, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.SetRemoveAsync(RedisKey, RedisValue, CommandFlags)"/>
		public async Task<TElement[]> MembersAsync(CommandFlags flags = CommandFlags.None)
		{
			RedisValue[] values = await GetDatabase().SetMembersAsync(Key, flags);
			return Array.ConvertAll(values, (Converter<RedisValue, TElement>)(x => RedisSerializer.Deserialize<TElement>(x)!));
		}

		/// <inheritdoc cref="IDatabaseAsync.SetPopAsync(RedisKey, CommandFlags)"/>
		public async Task<TElement> PopAsync(CommandFlags flags = CommandFlags.None)
		{
			RedisValue value = await GetDatabase().SetPopAsync(Key, flags);
			return value.IsNull ? default! : RedisSerializer.Deserialize<TElement>(value)!;
		}

		/// <inheritdoc cref="IDatabaseAsync.SetPopAsync(RedisKey, Int64, CommandFlags)"/>
		public async Task<TElement[]> PopAsync(long count, CommandFlags flags = CommandFlags.None)
		{
			RedisValue[] values = await GetDatabase().SetPopAsync(Key, count, flags);
			return Array.ConvertAll<RedisValue, TElement>(values, x => RedisSerializer.Deserialize<TElement>(x)!);
		}

		/// <inheritdoc cref="IDatabaseAsync.SetRemoveAsync(RedisKey, RedisValue, CommandFlags)"/>
		public Task<bool> RemoveAsync(TElement item, CommandFlags flags = CommandFlags.None)
		{
			return GetDatabase().SetRemoveAsync(Key, RedisSerializer.Serialize(item), flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.SetRemoveAsync(RedisKey, RedisValue[], CommandFlags)"/>
		public Task<long> RemoveAsync(IEnumerable<TElement> items, CommandFlags flags = CommandFlags.None)
		{
			RedisValue[] values = items.Select(x => RedisSerializer.Serialize(x)).ToArray();
			return GetDatabase().SetRemoveAsync(Key, values, flags);
		}
	}

	/// <summary>
	/// Extension methods for sets
	/// </summary>
	public static class RedisSetExtensions
	{
		/// <summary>
		/// Creates a version of this set which modifies a transaction rather than the direct DB
		/// </summary>
		public static RedisSet<TElement> With<TElement>(this ITransaction transaction, RedisSet<TElement> set)
		{
			return new RedisSet<TElement>(transaction, set.Key);
		}
	}
}
