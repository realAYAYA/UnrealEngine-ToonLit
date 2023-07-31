// Copyright Epic Games, Inc. All Rights Reserved.

using StackExchange.Redis;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace EpicGames.Redis
{
	/// <summary>
	/// Typed implementation of <see cref="SortedSetEntry"/>
	/// </summary>
	/// <typeparam name="T">The element type</typeparam>
	public readonly struct SortedSetEntry<T>
	{
		/// <summary>
		/// Accessor for the element type
		/// </summary>
		public readonly T Element { get; }

		/// <summary>
		/// The encoded element value
		/// </summary>
		public readonly RedisValue ElementValue { get; }

		/// <summary>
		/// Score for the entry
		/// </summary>
		public readonly double Score { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="entry"></param>
		public SortedSetEntry(SortedSetEntry entry)
		{
			Element = RedisSerializer.Deserialize<T>(entry.Element)!;
			ElementValue = entry.Element;
			Score = entry.Score;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="element"></param>
		/// <param name="score"></param>
		public SortedSetEntry(T element, double score)
		{
			Element = element;
			ElementValue = RedisSerializer.Serialize<T>(element);
			Score = score;
		}

		/// <summary>
		/// Deconstruct this item into a tuple
		/// </summary>
		/// <param name="outElement"></param>
		/// <param name="outScore"></param>
		public void Deconstruct(out T outElement, out double outScore)
		{
			outElement = Element;
			outScore = Score;
		}
	}

	/// <summary>
	/// Represents a typed Redis list with a given key
	/// </summary>
	/// <typeparam name="TElement">The type of element stored in the list</typeparam>
	public readonly struct RedisSortedSet<TElement>
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
		public RedisSortedSet(IDatabaseAsync database, RedisKey key)
		{
			Database = database;
			Key = key;
		}
		
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="connectionPool">Connection pool for Redis databases</param>
		/// <param name="key">Redis key this type is using</param>
		public RedisSortedSet(RedisConnectionPool connectionPool, RedisKey key)
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

		/// <inheritdoc cref="IDatabaseAsync.SortedSetAddAsync(RedisKey, RedisValue, Double, CommandFlags)"/>
		public Task<bool> AddAsync(TElement item, double score, When when = When.Always, CommandFlags flags = CommandFlags.None)
		{
			RedisValue value = RedisSerializer.Serialize(item);
			return GetDatabase().SortedSetAddAsync(Key, value, score, when, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.SortedSetAddAsync(RedisKey, RedisValue, Double, When, CommandFlags)"/>
		public Task<long> AddAsync(SortedSetEntry<TElement>[] values, When when = When.Always, CommandFlags flags = CommandFlags.None)
		{
			SortedSetEntry[] untyped = Array.ConvertAll(values, x => new SortedSetEntry(x.ElementValue, x.Score));
			return GetDatabase().SortedSetAddAsync(Key, untyped, when, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.SortedSetLengthAsync(RedisKey, Double, Double, Exclude, CommandFlags)"/>
		public Task<long> LengthAsync(double min = Double.NegativeInfinity, double max = Double.PositiveInfinity, Exclude exclude = Exclude.None, CommandFlags flags = CommandFlags.None)
		{
			return GetDatabase().SortedSetLengthAsync(Key, min, max, exclude, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.SortedSetLengthByValueAsync(RedisKey, RedisValue, RedisValue, Exclude, CommandFlags)"/>
		public Task<long> LengthByValueAsync(TElement min, TElement max, Exclude exclude = Exclude.None, CommandFlags flags = CommandFlags.None)
		{
			RedisValue minValue = RedisSerializer.Serialize(min);
			RedisValue maxValue = RedisSerializer.Serialize(max);
			return GetDatabase().SortedSetLengthByValueAsync(Key, minValue, maxValue, exclude, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.SortedSetRangeByRankAsync(RedisKey, Int64, Int64, Order, CommandFlags)"/>
		public async Task<TElement[]> RangeByRankAsync(long start, long stop = -1, Order order = Order.Ascending, CommandFlags flags = CommandFlags.None)
		{
			RedisValue[] values = await GetDatabase().SortedSetRangeByRankAsync(Key, start, stop, order, flags);
			return Array.ConvertAll(values, x => RedisSerializer.Deserialize<TElement>(x)!);
		}

		/// <inheritdoc cref="IDatabaseAsync.SortedSetRangeByRankWithScoresAsync(RedisKey, Int64, Int64, Order, CommandFlags)"/>
		public async Task<SortedSetEntry<TElement>[]> RangeByRankWithScoresAsync(long start, long stop = -1, Order order = Order.Ascending, CommandFlags flags = CommandFlags.None)
		{
			SortedSetEntry[] values = await GetDatabase().SortedSetRangeByRankWithScoresAsync(Key, start, stop, order, flags);
			return Array.ConvertAll(values, x => new SortedSetEntry<TElement>(x));
		}

		/// <inheritdoc cref="IDatabaseAsync.SortedSetRangeByScoreAsync(RedisKey, Double, Double, Exclude, Order, Int64, Int64, CommandFlags)"/>
		public async Task<TElement[]> RangeByScoreAsync(double start = Double.NegativeInfinity, double stop = Double.PositiveInfinity, Exclude exclude = Exclude.None, Order order = Order.Ascending, long skip = 0L, long take = -1L, CommandFlags flags = CommandFlags.None)
		{
			RedisValue[] values = await GetDatabase().SortedSetRangeByScoreAsync(Key, start, stop, exclude, order, skip, take, flags);
			return Array.ConvertAll(values, x => RedisSerializer.Deserialize<TElement>(x)!);
		}

		/// <inheritdoc cref="IDatabaseAsync.SortedSetRangeByScoreWithScoresAsync(RedisKey, Double, Double, Exclude, Order, Int64, Int64, CommandFlags)"/>
		public async Task<SortedSetEntry<TElement>[]> RangeByScoreWithScoresAsync(double start = Double.NegativeInfinity, double stop = Double.PositiveInfinity, Exclude exclude = Exclude.None, Order order = Order.Ascending, long skip = 0L, long take = -1L, CommandFlags flags = CommandFlags.None)
		{
			SortedSetEntry[] values = await GetDatabase().SortedSetRangeByScoreWithScoresAsync(Key, start, stop, exclude, order, skip, take, flags);
			return Array.ConvertAll(values, x => new SortedSetEntry<TElement>(x));
		}

		/// <inheritdoc cref="IDatabaseAsync.SortedSetRangeByValueAsync(RedisKey, RedisValue, RedisValue, Exclude, Order, Int64, Int64, CommandFlags)"/>
		public async Task<TElement[]> RangeByValueAsync(TElement min, TElement max, Exclude exclude = Exclude.None, Order order = Order.Ascending, long skip = 0L, long take = -1L, CommandFlags flags = CommandFlags.None)
		{
			RedisValue minValue = RedisSerializer.Serialize(min);
			RedisValue maxValue = RedisSerializer.Serialize(max);
			RedisValue[] values = await GetDatabase().SortedSetRangeByValueAsync(Key, minValue, maxValue, exclude, order, skip, take, flags);
			return Array.ConvertAll(values, x => RedisSerializer.Deserialize<TElement>(x)!);
		}

		/// <inheritdoc cref="IDatabaseAsync.SortedSetRankAsync(RedisKey, RedisValue, Order, CommandFlags)"/>
		public Task<long?> RankAsync(TElement item, Order order = Order.Ascending, CommandFlags flags = CommandFlags.None)
		{
			return GetDatabase().SortedSetRankAsync(Key, RedisSerializer.Serialize(item), order, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.SortedSetRemoveAsync(RedisKey, RedisValue, CommandFlags)"/>
		public Task<bool> RemoveAsync(TElement value, CommandFlags flags = CommandFlags.None)
		{
			return GetDatabase().SortedSetRemoveAsync(Key, RedisSerializer.Serialize(value), flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.SortedSetRemoveAsync(RedisKey, RedisValue[], CommandFlags)"/>
		public Task<long> RemoveAsync(IEnumerable<TElement> items, CommandFlags flags = CommandFlags.None)
		{
			RedisValue[] values = items.Select(x => RedisSerializer.Serialize(x)).ToArray();
			return GetDatabase().SortedSetRemoveAsync(Key, values, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.SortedSetRemoveRangeByRankAsync(RedisKey, Int64, Int64, CommandFlags)"/>
		public Task<long> RemoveRangeByRankAsync(long start, long stop, CommandFlags flags = CommandFlags.None)
		{
			return GetDatabase().SortedSetRemoveRangeByRankAsync(Key, start, stop, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.SortedSetRemoveRangeByScoreAsync(RedisKey, Double, Double, Exclude, CommandFlags)"/>
		public Task<long> RemoveRangeByScoreAsync(double start, double stop, Exclude exclude = Exclude.None, CommandFlags flags = CommandFlags.None)
		{
			return GetDatabase().SortedSetRemoveRangeByScoreAsync(Key, start, stop, exclude, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.SortedSetRemoveRangeByValueAsync(RedisKey, RedisValue, RedisValue, Exclude, CommandFlags)"/>
		public async Task<long> RemoveRangeByValueAsync(TElement min, TElement max, Exclude exclude = Exclude.None, CommandFlags flags = CommandFlags.None)
		{
			RedisValue minValue = RedisSerializer.Serialize(min);
			RedisValue maxValue = RedisSerializer.Serialize(max);
			return await GetDatabase().SortedSetRemoveRangeByValueAsync(Key, minValue, maxValue, exclude, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.SortedSetAddAsync(RedisKey, RedisValue, Double, CommandFlags)"/>
		public async IAsyncEnumerable<SortedSetEntry<TElement>> ScanAsync(RedisValue pattern = default, int pageSize = 250, long cursor = 0, int pageOffset = 0, CommandFlags flags = CommandFlags.None)
		{
			await foreach (SortedSetEntry entry in GetDatabase().SortedSetScanAsync(Key, pattern, pageSize, cursor, pageOffset, flags))
			{
				yield return new SortedSetEntry<TElement>(entry);
			}
		}

		/// <inheritdoc cref="IDatabaseAsync.SortedSetScoreAsync(RedisKey, RedisValue, CommandFlags)"/>
		public Task<double?> ScoreAsync(TElement member, CommandFlags flags = CommandFlags.None)
		{
			RedisValue value = RedisSerializer.Serialize(member);
			return GetDatabase().SortedSetScoreAsync(Key, value, flags);
		}
	}

	/// <summary>
	/// Extension methods for sets
	/// </summary>
	public static class RedisSortedSetExtensions
	{
		/// <summary>
		/// Creates a version of this set which modifies a transaction rather than the direct DB
		/// </summary>
		public static RedisSortedSet<TElement> With<TElement>(this ITransaction transaction, RedisSortedSet<TElement> set)
		{
			return new RedisSortedSet<TElement>(transaction, set.Key);
		}
	}
}
