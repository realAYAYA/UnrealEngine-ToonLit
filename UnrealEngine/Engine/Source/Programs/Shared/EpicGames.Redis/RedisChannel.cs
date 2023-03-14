// Copyright Epic Games, Inc. All Rights Reserved.

using StackExchange.Redis;
using System;
using System.Threading.Tasks;

namespace EpicGames.Redis
{
	using RedisChannel = StackExchange.Redis.RedisChannel;

	/// <summary>
	/// Represents a typed pub/sub channel with a particular value
	/// </summary>
	/// <typeparam name="T">The type of element stored in the channel</typeparam>
	public readonly struct RedisChannel<T> : IEquatable<RedisChannel<T>>
	{
		/// <summary>
		/// The key for the list
		/// </summary>
		public readonly RedisChannel Channel { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="channel"></param>
		public RedisChannel(RedisChannel channel) => Channel = channel;

		/// <inheritdoc/>
		public override bool Equals(object? obj) => obj is RedisChannel<T> list && Channel == list.Channel;

		/// <inheritdoc/>
		public bool Equals(RedisChannel<T> other) => Channel == other.Channel;

		/// <inheritdoc/>
		public override int GetHashCode() => Channel.GetHashCode();

		/// <summary>Compares two instances for equality</summary>
		public static bool operator ==(RedisChannel<T> left, RedisChannel<T> right) => left.Channel == right.Channel;

		/// <summary>Compares two instances for equality</summary>
		public static bool operator !=(RedisChannel<T> left, RedisChannel<T> right) => left.Channel != right.Channel;
	}

	/// <summary>
	/// Subscription to a <see cref="RedisChannel{Task}"/>
	/// </summary>
	/// <typeparam name="T"></typeparam>
	public sealed class RedisChannelSubscription<T> : IDisposable, IAsyncDisposable
	{
		/// <summary>
		/// The subscriber to register with
		/// </summary>
		ISubscriber? _subscriber;

		/// <summary>
		/// The channel to post on
		/// </summary>
		public RedisChannel<T> Channel { get; }

		/// <summary>
		/// The handler to call
		/// </summary>
		readonly Action<RedisChannel<T>, T> _handler;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="subscriber"></param>
		/// <param name="channel"></param>
		/// <param name="handler"></param>
		RedisChannelSubscription(ISubscriber subscriber, RedisChannel<T> channel, Action<RedisChannel<T>, T> handler)
		{
			_subscriber = subscriber;
			Channel = channel;
			_handler = handler;
		}
		
		/// <summary>
		/// Start the subscription
		/// </summary>
		internal static async Task<RedisChannelSubscription<T>> CreateAsync(ISubscriber subscriber, RedisChannel<T> channel, Action<RedisChannel<T>, T> handler, CommandFlags flags = CommandFlags.None)
		{
			RedisChannelSubscription<T> subscription = new RedisChannelSubscription<T>(subscriber, channel, handler);
			await subscription._subscriber!.SubscribeAsync(channel.Channel, subscription.UntypedHandler, flags);
			return subscription;
		}

		/// <summary>
		/// Unsubscribe from the channel
		/// </summary>
		/// <param name="flags">Flags for the operation</param>
		public async Task UnsubscribeAsync(CommandFlags flags = CommandFlags.None)
		{
			if (_subscriber != null)
			{
				await _subscriber.UnsubscribeAsync(Channel.Channel, UntypedHandler, flags);
				_subscriber = null;
			}
		}

		/// <summary>
		/// The callback for messages being received
		/// </summary>
		/// <param name="_"></param>
		/// <param name="message"></param>
		void UntypedHandler(RedisChannel _, RedisValue message)
		{
			_handler(Channel, RedisSerializer.Deserialize<T>(message)!);
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			await UnsubscribeAsync();
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			UnsubscribeAsync().Wait();
		}
	}

	/// <summary>
	/// Extension methods for typed lists
	/// </summary>
	public static class RedisChannelExtensions
	{
		/// <inheritdoc cref="ISubscriber.SubscribeAsync(StackExchange.Redis.RedisChannel, Action{StackExchange.Redis.RedisChannel, RedisValue}, CommandFlags)"/>
		public static Task<RedisChannelSubscription<T>> SubscribeAsync<T>(this ISubscriber subscriber, RedisChannel<T> channel, Action<RedisChannel<T>, T> handler, CommandFlags flags = CommandFlags.None)
		{
			return RedisChannelSubscription<T>.CreateAsync(subscriber, channel, handler, flags);
		}

		/// <inheritdoc cref="IDatabaseAsync.PublishAsync(StackExchange.Redis.RedisChannel, RedisValue, CommandFlags)"/>
		public static Task PublishAsync<T>(this IDatabaseAsync database, RedisChannel<T> channel, T item, CommandFlags flags = CommandFlags.None)
		{
			RedisValue value = RedisSerializer.Serialize(item);
			return database.PublishAsync(channel.Channel, value, flags);
		}
	}
}
