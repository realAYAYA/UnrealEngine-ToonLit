// Copyright Epic Games, Inc. All Rights Reserved.

using StackExchange.Redis;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace EpicGames.Redis
{
	/// <summary>
	/// Represents a typed Redis hash with a given key and value
	/// </summary>
	/// <typeparam name="TName">The key type for the hash</typeparam>
	/// <typeparam name="TValue">The value type for the hash</typeparam>
	public readonly struct RedisHash<TName, TValue>
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
		public RedisHash(IDatabaseAsync database, RedisKey key)
		{
			Database = database;
			Key = key;
		}
		
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="connectionPool">Connection pool for Redis databases</param>
		/// <param name="key">Redis key this type is using</param>
		public RedisHash(RedisConnectionPool connectionPool, RedisKey key)
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

		/// <inheritdoc cref="IDatabaseAsync.HashDeleteAsync(RedisKey, RedisValue, CommandFlags)"/>
		public Task<bool> DeleteAsync(TName name, CommandFlags flags = CommandFlags.None)
		{
			return GetDatabase().HashDeleteAsync(Key, RedisSerializer.Serialize(name), flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.HashDeleteAsync(RedisKey, RedisValue[], CommandFlags)"/>
		public Task<long> DeleteAsync(IEnumerable<TName> names, CommandFlags flags = CommandFlags.None)
		{
			RedisValue[] nameArray = names.Select(x => RedisSerializer.Serialize(x)).ToArray();
			return GetDatabase().HashDeleteAsync(Key, nameArray, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.HashExistsAsync(RedisKey, RedisValue, CommandFlags)"/>
		public Task<bool> ExistsAsync(TName name, CommandFlags flags = CommandFlags.None)
		{
			return GetDatabase().HashExistsAsync(Key, RedisSerializer.Serialize(name), flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.HashGetAsync(RedisKey, RedisValue, CommandFlags)"/>
		public async Task<TValue> GetAsync(TName name, CommandFlags flags = CommandFlags.None)
		{
			RedisValue value = await GetDatabase().HashGetAsync(Key, RedisSerializer.Serialize(name), flags);
			return value.IsNull ? default! : RedisSerializer.Deserialize<TValue>(value)!;
		}

		/// <inheritdoc cref="IDatabaseAsync.HashGetAsync(RedisKey, RedisValue[], CommandFlags)"/>
		public async Task<TValue[]> GetAsync(IEnumerable<TName> names, CommandFlags flags = CommandFlags.None)
		{
			RedisValue[] nameArray = names.Select(x => RedisSerializer.Serialize(x)).ToArray();
			RedisValue[] valueArray = await GetDatabase().HashGetAsync(Key, nameArray, flags);
			return Array.ConvertAll(valueArray, x => RedisSerializer.Deserialize<TValue>(x)!);
		}

		/// <inheritdoc cref="IDatabaseAsync.HashGetAllAsync(RedisKey, CommandFlags)"/>
		public async Task<HashEntry<TName, TValue>[]> GetAllAsync(CommandFlags flags = CommandFlags.None)
		{
			HashEntry[] entries = await GetDatabase().HashGetAllAsync(Key, flags);
			return Array.ConvertAll(entries, x => new HashEntry<TName, TValue>(RedisSerializer.Deserialize<TName>(x.Name)!, RedisSerializer.Deserialize<TValue>(x.Value)!));
		}

		/// <inheritdoc cref="IDatabaseAsync.HashKeysAsync(RedisKey, CommandFlags)"/>
		public async Task<TName[]> KeysAsync(CommandFlags flags = CommandFlags.None)
		{
			RedisValue[] nameArray = await GetDatabase().HashKeysAsync(Key, flags);
			return Array.ConvertAll(nameArray, x => RedisSerializer.Deserialize<TName>(x)!);
		}

		/// <inheritdoc cref="IDatabaseAsync.HashLengthAsync(RedisKey, CommandFlags)"/>
		public Task<long> LengthAsync(CommandFlags flags = CommandFlags.None)
		{
			return GetDatabase().HashLengthAsync(Key, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.HashSetAsync(RedisKey, RedisValue, RedisValue, When, CommandFlags)"/>
		public Task SetAsync(TName name, TValue value, When when = When.Always, CommandFlags flags = CommandFlags.None)
		{
			return GetDatabase().HashSetAsync(Key, RedisSerializer.Serialize(name), RedisSerializer.Serialize(value), when, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.HashSetAsync(RedisKey, HashEntry[], CommandFlags)"/>
		public Task SetAsync(IEnumerable<HashEntry<TName, TValue>> entries, CommandFlags flags = CommandFlags.None)
		{
			return GetDatabase().HashSetAsync(Key, entries.Select(x => (HashEntry)x).ToArray(), flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.HashValuesAsync(RedisKey, CommandFlags)"/>
		public async Task<TValue[]> ValuesAsync(CommandFlags flags = CommandFlags.None)
		{
			RedisValue[] values = await GetDatabase().HashValuesAsync(Key, flags);
			return Array.ConvertAll(values, x => RedisSerializer.Deserialize<TValue>(x)!);
		}
	}

	/// <inheritdoc cref="HashEntry"/>
	public readonly struct HashEntry<TName, TValue>
	{
		/// <inheritdoc cref="HashEntry.Name"/>
		public readonly TName Name { get; }

		/// <inheritdoc cref="HashEntry.Value"/>
		public readonly TValue Value { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="name"></param>
		/// <param name="value"></param>
		public HashEntry(TName name, TValue value)
		{
			Name = name;
			Value = value;
		}

		/// <summary>
		/// Implicit conversion to a <see cref="HashEntry"/>
		/// </summary>
		/// <param name="entry"></param>
		public static implicit operator HashEntry(HashEntry<TName, TValue> entry)
		{
			return new HashEntry(RedisSerializer.Serialize(entry.Name), RedisSerializer.Serialize(entry.Value));
		}

		/// <summary>
		/// Implicit conversion to a <see cref="KeyValuePair{TName, TValue}"/>
		/// </summary>
		/// <param name="entry"></param>
		public static implicit operator KeyValuePair<TName, TValue>(HashEntry<TName, TValue> entry)
		{
			return new KeyValuePair<TName, TValue>(entry.Name, entry.Value);
		}
	}

	/// <summary>
	/// Extension methods for hashes
	/// </summary>
	public static class RedisHashExtensions
	{
		/// <inheritdoc cref="IDatabaseAsync.HashDecrementAsync(RedisKey, RedisValue, Int64, CommandFlags)"/>
		public static Task<long> DecrementAsync<TName>(this RedisHash<TName, long> hash, TName name, long value = 1L, CommandFlags flags = CommandFlags.None)
		{
			return hash.GetDatabase().HashDecrementAsync(hash.Key, RedisSerializer.Serialize<TName>(name), value, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.HashDecrementAsync(RedisKey, RedisValue, Double, CommandFlags)"/>
		public static Task<double> DecrementAsync<TName>(this RedisHash<TName, long> hash, TName name, double value = 1.0, CommandFlags flags = CommandFlags.None)
		{
			return hash.GetDatabase().HashDecrementAsync(hash.Key, RedisSerializer.Serialize<TName>(name), value, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.HashIncrementAsync(RedisKey, RedisValue, Int64, CommandFlags)"/>
		public static Task<long> IncrementAsync<TName>(this RedisHash<TName, long> hash, TName name, long value = 1L, CommandFlags flags = CommandFlags.None)
		{
			return hash.GetDatabase().HashIncrementAsync(hash.Key, RedisSerializer.Serialize<TName>(name), value, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.HashIncrementAsync(RedisKey, RedisValue, Double, CommandFlags)"/>
		public static Task<double> IncrementAsync<TName>(this RedisHash<TName, long> hash, TName name, double value = 1.0, CommandFlags flags = CommandFlags.None)
		{
			return hash.GetDatabase().HashIncrementAsync(hash.Key, RedisSerializer.Serialize<TName>(name), value, flags);
		}

		/// <summary>
		/// Creates a version of this set which modifies a transaction rather than the direct DB
		/// </summary>
		public static RedisHash<TName, TValue> With<TName, TValue>(this ITransaction transaction, RedisHash<TName, TValue> set)
		{
			return new RedisHash<TName, TValue>(transaction, set.Key);
		}
	}
}
