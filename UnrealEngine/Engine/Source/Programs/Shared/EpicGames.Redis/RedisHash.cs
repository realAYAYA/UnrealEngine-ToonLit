// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using StackExchange.Redis;

namespace EpicGames.Redis
{
	/// <summary>
	/// Represents a typed Redis list with a given key
	/// </summary>
	/// <typeparam name="TName">Type of the hash key</typeparam>
	/// <typeparam name="TValue">Type of the hash value</typeparam>
	public readonly struct RedisHashKey<TName, TValue>
	{
		/// <summary>
		/// The key for the list
		/// </summary>
		public readonly RedisKey Inner { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="inner">Redis key this type is using</param>
		public RedisHashKey(RedisKey inner)
		{
			Inner = inner;
		}

		/// <summary>
		/// Implicit conversion to typed redis key.
		/// </summary>
		/// <param name="key">Key to convert</param>
		public static implicit operator RedisHashKey<TName, TValue>(string key) => new RedisHashKey<TName, TValue>(new RedisKey(key));

		/// <summary>
		/// Implicit conversion to untyped redis keys.
		/// </summary>
		/// <param name="key">Key to convert</param>
		public static implicit operator TypedRedisKey(RedisHashKey<TName, TValue> key) => new TypedRedisKey(key.Inner);
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
		#region HashDecrementAsync

		/// <inheritdoc cref="IDatabaseAsync.HashDecrementAsync(RedisKey, RedisValue, Int64, CommandFlags)"/>
		public static Task<long> HashDecrementAsync<TName>(this IDatabaseAsync target, RedisHashKey<TName, long> key, TName name, long value = 1L, CommandFlags flags = CommandFlags.None)
		{
			return target.HashDecrementAsync(key.Inner, RedisSerializer.Serialize<TName>(name), value, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.HashDecrementAsync(RedisKey, RedisValue, Double, CommandFlags)"/>
		public static Task<double> HashDecrementAsync<TName>(this IDatabaseAsync target, RedisHashKey<TName, long> key, TName name, double value = 1.0, CommandFlags flags = CommandFlags.None)
		{
			return target.HashDecrementAsync(key.Inner, RedisSerializer.Serialize<TName>(name), value, flags);
		}

		#endregion

		#region HashDeleteAsync

		/// <inheritdoc cref="IDatabaseAsync.HashDeleteAsync(RedisKey, RedisValue, CommandFlags)"/>
		public static Task<bool> HashDeleteAsync<TName, TValue>(this IDatabaseAsync target, RedisHashKey<TName, TValue> key, TName name, CommandFlags flags = CommandFlags.None)
		{
			return target.HashDeleteAsync(key.Inner, RedisSerializer.Serialize(name), flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.HashDeleteAsync(RedisKey, RedisValue[], CommandFlags)"/>
		public static Task<long> HashDeleteAsync<TName, TValue>(this IDatabaseAsync target, RedisHashKey<TName, TValue> key, TName[] names, CommandFlags flags = CommandFlags.None)
		{
			return target.HashDeleteAsync(key.Inner, RedisSerializer.Serialize(names), flags);
		}

		#endregion

		#region HashExistsAsync

		/// <inheritdoc cref="IDatabaseAsync.HashExistsAsync(RedisKey, RedisValue, CommandFlags)"/>
		public static Task<bool> HashExistsAsync<TName, TValue>(this IDatabaseAsync target, RedisHashKey<TName, TValue> key, TName name, CommandFlags flags = CommandFlags.None)
		{
			return target.HashExistsAsync(key.Inner, RedisSerializer.Serialize(name), flags);
		}

		#endregion

		#region HashGetAsync

		/// <inheritdoc cref="IDatabaseAsync.HashGetAsync(RedisKey, RedisValue, CommandFlags)"/>
		public static Task<TValue> HashGetAsync<TName, TValue>(this IDatabaseAsync target, RedisHashKey<TName, TValue> key, TName name, CommandFlags flags = CommandFlags.None)
		{
			return target.HashGetAsync(key.Inner, RedisSerializer.Serialize(name), flags).DeserializeAsync<TValue>();
		}

		/// <inheritdoc cref="IDatabaseAsync.HashGetAsync(RedisKey, RedisValue[], CommandFlags)"/>
		public static Task<TValue[]> HashGetAsync<TName, TValue>(this IDatabaseAsync target, RedisHashKey<TName, TValue> key, TName[] names, CommandFlags flags = CommandFlags.None)
		{
			return target.HashGetAsync(key.Inner, RedisSerializer.Serialize(names), flags).DeserializeAsync<TValue>();
		}

		#endregion

		#region HashGetAllAsync

		/// <inheritdoc cref="IDatabaseAsync.HashGetAllAsync(RedisKey, CommandFlags)"/>
		public static async Task<HashEntry<TName, TValue>[]> HashGetAllAsync<TName, TValue>(this IDatabaseAsync target, RedisHashKey<TName, TValue> key, CommandFlags flags = CommandFlags.None)
		{
			HashEntry[] entries = await target.HashGetAllAsync(key.Inner, flags);
			return Array.ConvertAll(entries, x => new HashEntry<TName, TValue>(RedisSerializer.Deserialize<TName>(x.Name)!, RedisSerializer.Deserialize<TValue>(x.Value)!));
		}

		#endregion

		#region HashIncrementAsync

		/// <inheritdoc cref="IDatabaseAsync.HashIncrementAsync(RedisKey, RedisValue, Int64, CommandFlags)"/>
		public static Task<long> HashIncrementAsync<TName>(this IDatabaseAsync target, RedisHashKey<TName, long> key, TName name, long value = 1L, CommandFlags flags = CommandFlags.None)
		{
			return target.HashIncrementAsync(key.Inner, RedisSerializer.Serialize<TName>(name), value, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.HashIncrementAsync(RedisKey, RedisValue, Double, CommandFlags)"/>
		public static Task<double> HashIncrementAsync<TName>(this IDatabaseAsync target, RedisHashKey<TName, long> key, TName name, double value = 1.0, CommandFlags flags = CommandFlags.None)
		{
			return target.HashIncrementAsync(key.Inner, RedisSerializer.Serialize<TName>(name), value, flags);
		}

		#endregion

		#region HashKeysAsync

		/// <inheritdoc cref="IDatabaseAsync.HashKeysAsync(RedisKey, CommandFlags)"/>
		public static Task<TName[]> HashKeysAsync<TName, TValue>(this IDatabaseAsync target, RedisHashKey<TName, TValue> key, CommandFlags flags = CommandFlags.None)
		{
			return target.HashKeysAsync(key.Inner, flags).DeserializeAsync<TName>();
		}

		#endregion

		#region HashLengthAsync

		/// <inheritdoc cref="IDatabaseAsync.HashLengthAsync(RedisKey, CommandFlags)"/>
		public static Task<long> HashLengthAsync(this IDatabaseAsync target, TypedRedisKey key, CommandFlags flags = CommandFlags.None)
		{
			return target.HashLengthAsync(key, flags);
		}

		#endregion

		#region HashSetAsync

		/// <inheritdoc cref="IDatabaseAsync.HashSetAsync(RedisKey, RedisValue, RedisValue, When, CommandFlags)"/>
		public static Task HashSetAsync<TName, TValue>(this IDatabaseAsync target, RedisHashKey<TName, TValue> key, TName name, TValue value, When when = When.Always, CommandFlags flags = CommandFlags.None)
		{
			return target.HashSetAsync(key.Inner, RedisSerializer.Serialize(name), RedisSerializer.Serialize(value), when, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.HashSetAsync(RedisKey, HashEntry[], CommandFlags)"/>
		public static Task HashSetAsync<TName, TValue>(this IDatabaseAsync target, RedisHashKey<TName, TValue> key, IEnumerable<HashEntry<TName, TValue>> entries, CommandFlags flags = CommandFlags.None)
		{
			return target.HashSetAsync(key.Inner, entries.Select(x => (HashEntry)x).ToArray(), flags);
		}

		#endregion

		#region HashValuesAsync

		/// <inheritdoc cref="IDatabaseAsync.HashValuesAsync(RedisKey, CommandFlags)"/>
		public static Task<TValue[]> HashValuesAsync<TName, TValue>(this IDatabaseAsync target, RedisHashKey<TName, TValue> key, CommandFlags flags = CommandFlags.None)
		{
			return target.HashValuesAsync(key.Inner, flags).DeserializeAsync<TValue>();
		}

		#endregion
	}
}
