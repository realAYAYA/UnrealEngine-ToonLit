// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading.Tasks;
using StackExchange.Redis;

namespace EpicGames.Redis
{
	/// <summary>
	/// Represents a typed Redis set with a given key
	/// </summary>
	/// <typeparam name="TElement">The type of element stored in the set</typeparam>
	public readonly struct RedisSetKey<TElement>
	{
		/// <summary>
		/// The key for the list
		/// </summary>
		public readonly RedisKey Inner { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="inner">Redis key this type is using</param>
		public RedisSetKey(RedisKey inner)
		{
			Inner = inner;
		}

		/// <summary>
		/// Implicit conversion to typed redis key.
		/// </summary>
		/// <param name="key">Key to convert</param>
		public static implicit operator RedisSetKey<TElement>(string key) => new RedisSetKey<TElement>(new RedisKey(key));

		/// <summary>
		/// Implicit conversion to untyped redis keys.
		/// </summary>
		/// <param name="key">Key to convert</param>
		public static implicit operator TypedRedisKey(RedisSetKey<TElement> key) => key.Inner;
	}

	/// <summary>
	/// Extension methods for sets
	/// </summary>
	public static class RedisSetExtensions
	{
		#region SetAddAsync

		/// <inheritdoc cref="IDatabaseAsync.SetAddAsync(RedisKey, RedisValue, CommandFlags)"/>
		public static Task<bool> SetAddAsync<TElement>(this IDatabaseAsync target, RedisSetKey<TElement> key, TElement item, CommandFlags flags = CommandFlags.None)
		{
			return target.SetAddAsync(key.Inner, RedisSerializer.Serialize(item), flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.SetAddAsync(RedisKey, RedisValue[], CommandFlags)"/>
		public static Task<long> SetAddAsync<TElement>(this IDatabaseAsync target, RedisSetKey<TElement> key, TElement[] values, CommandFlags flags = CommandFlags.None)
		{
			return target.SetAddAsync(key.Inner, RedisSerializer.Serialize(values), flags);
		}

		#endregion

		#region SetContainsAsync

		/// <inheritdoc cref="IDatabaseAsync.SetContainsAsync(RedisKey, RedisValue, CommandFlags)"/>
		public static Task<bool> SetContainsAsync<TElement>(this IDatabaseAsync target, RedisSetKey<TElement> key, TElement value, CommandFlags flags = CommandFlags.None)
		{
			return target.SetContainsAsync(key.Inner, RedisSerializer.Serialize(value), flags);
		}

		#endregion

		#region SetLengthAsync

		/// <inheritdoc cref="IDatabaseAsync.SetLengthAsync(RedisKey, CommandFlags)"/>
		public static Task<long> SetLengthAsync<TElement>(this IDatabaseAsync target, RedisSetKey<TElement> key, CommandFlags flags = CommandFlags.None)
		{
			return target.SetLengthAsync(key.Inner, flags);
		}

		#endregion

		#region SetMembersAsync

		/// <inheritdoc cref="IDatabaseAsync.SetMembersAsync(RedisKey, CommandFlags)"/>
		public static Task<TElement[]> SetMembersAsync<TElement>(this IDatabaseAsync target, RedisSetKey<TElement> key, CommandFlags flags = CommandFlags.None)
		{
			return target.SetMembersAsync(key.Inner, flags).DeserializeAsync<TElement>();
		}

		#endregion

		#region SetPopAsync

		/// <inheritdoc cref="IDatabaseAsync.SetPopAsync(RedisKey, CommandFlags)"/>
		public static Task<TElement> SetPopAsync<TElement>(this IDatabaseAsync target, RedisSetKey<TElement> key, CommandFlags flags = CommandFlags.None)
		{
			return target.SetPopAsync(key.Inner, flags).DeserializeAsync<TElement>();
		}

		/// <inheritdoc cref="IDatabaseAsync.SetPopAsync(RedisKey, Int64, CommandFlags)"/>
		public static Task<TElement[]> SetPopAsync<TElement>(this IDatabaseAsync target, RedisSetKey<TElement> key, long count, CommandFlags flags = CommandFlags.None)
		{
			return target.SetPopAsync(key.Inner, count, flags).DeserializeAsync<TElement>();
		}

		#endregion

		#region SetRemoveAsync

		/// <inheritdoc cref="IDatabaseAsync.SetRemoveAsync(RedisKey, RedisValue, CommandFlags)"/>
		public static Task<bool> SetRemoveAsync<TElement>(this IDatabaseAsync target, RedisSetKey<TElement> key, TElement item, CommandFlags flags = CommandFlags.None)
		{
			return target.SetRemoveAsync(key.Inner, RedisSerializer.Serialize(item), flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.SetRemoveAsync(RedisKey, RedisValue[], CommandFlags)"/>
		public static Task<long> SetRemoveAsync<TElement>(this IDatabaseAsync target, RedisSetKey<TElement> key, TElement[] values, CommandFlags flags = CommandFlags.None)
		{
			return target.SetRemoveAsync(key.Inner, RedisSerializer.Serialize(values), flags);
		}

		#endregion
	}
}
