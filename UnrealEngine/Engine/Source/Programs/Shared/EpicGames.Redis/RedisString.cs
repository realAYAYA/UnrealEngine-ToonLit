// Copyright Epic Games, Inc. All Rights Reserved.

using StackExchange.Redis;
using System;
using System.Threading.Tasks;

namespace EpicGames.Redis
{
	/// <summary>
	/// Represents a typed Redis list with a given key
	/// </summary>
	/// <typeparam name="TElement">The type of element stored in the list</typeparam>
	public readonly struct RedisString<TElement>
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
		/// <param name="database"></param>
		/// <param name="key"></param>
		public RedisString(IDatabaseAsync database, RedisKey key)
		{
			Database = database;
			Key = key;
		}
		
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="connectionPool"></param>
		/// <param name="key"></param>
		public RedisString(RedisConnectionPool connectionPool, RedisKey key)
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

		/// <inheritdoc cref="IDatabaseAsync.StringLengthAsync(RedisKey, CommandFlags)"/>
		public Task<long> LengthAsync(CommandFlags flags = CommandFlags.None)
		{
			return GetDatabase().StringLengthAsync(Key, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.StringSetAsync(RedisKey, RedisValue, TimeSpan?, When, CommandFlags)"/>
		public Task<bool> SetAsync(TElement value, TimeSpan? expiry = null, When when = When.Always, CommandFlags flags = CommandFlags.None)
		{
			return GetDatabase().StringSetAsync(Key, RedisSerializer.Serialize(value), expiry, when, flags);
		}
	}

	/// <summary>
	/// Extension methods for sets
	/// </summary>
	public static class RedisStringExtensions
	{
		/// <inheritdoc cref="IDatabaseAsync.KeyDeleteAsync(RedisKey, CommandFlags)"/>
		public static Task<bool> DeleteAsync<TElement>(this RedisString<TElement> str, CommandFlags flags = CommandFlags.None)
		{
			return str.GetDatabase().KeyDeleteAsync(str.Key, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.StringGetAsync(RedisKey, CommandFlags)"/>
		public static async Task<TElement?> GetAsync<TElement>(this RedisString<TElement> str, CommandFlags flags = CommandFlags.None) where TElement : class
		{
			RedisValue value = await str.GetDatabase().StringGetAsync(str.Key, flags);
			if (value.IsNullOrEmpty)
			{
				return null;
			}
			return RedisSerializer.Deserialize<TElement>(value);
		}

		/// <inheritdoc cref="IDatabaseAsync.StringGetAsync(RedisKey, CommandFlags)"/>
		public static async Task<TElement?> GetValueAsync<TElement>(this RedisString<TElement> str, CommandFlags flags = CommandFlags.None) where TElement : struct
		{
			RedisValue value = await str.GetDatabase().StringGetAsync(str.Key, flags);
			if (value.IsNullOrEmpty)
			{
				return default(TElement);
			}
			return RedisSerializer.Deserialize<TElement>(value);
		}

		/// <inheritdoc cref="IDatabaseAsync.StringDecrementAsync(RedisKey, Int64, CommandFlags)"/>
		public static Task<long> DecrementAsync(this RedisString<long> str, long value = 1L, CommandFlags flags = CommandFlags.None)
		{
			return str.GetDatabase().StringDecrementAsync(str.Key, value, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.StringDecrementAsync(RedisKey, Double, CommandFlags)"/>
		public static Task<double> DecrementAsync(this RedisString<double> str, double value = 1.0, CommandFlags flags = CommandFlags.None)
		{
			return str.GetDatabase().StringDecrementAsync(str.Key, value, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.StringDecrementAsync(RedisKey, Double, CommandFlags)"/>
		public static Task<long> IncrementAsync(this RedisString<long> str, long value = 1L, CommandFlags flags = CommandFlags.None)
		{
			return str.GetDatabase().StringIncrementAsync(str.Key, value, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.StringDecrementAsync(RedisKey, Double, CommandFlags)"/>
		public static Task<double> IncrementAsync(this RedisString<double> str, double value = 1.0, CommandFlags flags = CommandFlags.None)
		{
			return str.GetDatabase().StringIncrementAsync(str.Key, value, flags);
		}

		/// <summary>
		/// Creates a version of this string which modifies a transaction rather than the direct DB
		/// </summary>
		public static RedisString<TElement> With<TElement>(this ITransaction transaction, RedisString<TElement> str)
		{
			return new RedisString<TElement>(transaction, str.Key);
		}
	}
}
