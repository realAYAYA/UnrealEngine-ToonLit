// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Redis.Utility;
using Horde.Build.Server;
using Microsoft.Extensions.Logging;
using StackExchange.Redis;

namespace HordeCommon
{
	/// <summary>
	/// Base interface for a scheduled event
	/// </summary>
	public interface ITicker : IDisposable
	{
		/// <summary>
		/// Start the ticker
		/// </summary>
		Task StartAsync();

		/// <summary>
		/// Stop the ticker
		/// </summary>
		Task StopAsync();
	}

	/// <summary>
	/// Placeholder interface for ITicker
	/// </summary>
	public sealed class NullTicker : ITicker
	{
		/// <inheritdoc/>
		public void Dispose() { }

		/// <inheritdoc/>
		public Task StartAsync() => Task.CompletedTask;

		/// <inheritdoc/>
		public Task StopAsync() => Task.CompletedTask;
	}

	/// <summary>
	/// Interface representing time and scheduling events which is pluggable during testing. In normal use, the Clock implementation below is used. 
	/// </summary>
	public interface IClock
	{
		/// <summary>
		/// Return time expressed as the Coordinated Universal Time (UTC)
		/// </summary>
		/// <returns></returns>
		DateTime UtcNow { get; }

		/// <summary>
		/// Create an event that will trigger after the given time
		/// </summary>
		/// <param name="name">Name of the event</param>
		/// <param name="interval">Time after which the event will trigger</param>
		/// <param name="tickAsync">Callback for the tick. Returns the time interval until the next tick, or null to cancel the tick.</param>
		/// <param name="logger">Logger for error messages</param>
		/// <returns>Handle to the event</returns>
		ITicker AddTicker(string name, TimeSpan interval, Func<CancellationToken, ValueTask<TimeSpan?>> tickAsync, ILogger logger);

		/// <summary>
		/// Create a ticker shared between all server pods
		/// </summary>
		/// <param name="name">Name of the event</param>
		/// <param name="interval">Time after which the event will trigger</param>
		/// <param name="tickAsync">Callback for the tick. Returns the time interval until the next tick, or null to cancel the tick.</param>
		/// <param name="logger">Logger for error messages</param>
		/// <returns>New ticker instance</returns>
		ITicker AddSharedTicker(string name, TimeSpan interval, Func<CancellationToken, ValueTask> tickAsync, ILogger logger);
	}

	/// <summary>
	/// Extension methods for <see cref="IClock"/>
	/// </summary>
	public static class ClockExtensions
	{
		/// <summary>
		/// Create an event that will trigger after the given time
		/// </summary>
		/// <param name="clock">Clock to schedule the event on</param>
		/// <param name="name">Name of the ticker</param>
		/// <param name="interval">Interval for the callback</param>
		/// <param name="tickAsync">Trigger callback</param>
		/// <param name="logger">Logger for any error messages</param>
		/// <returns>Handle to the event</returns>
		public static ITicker AddTicker(this IClock clock, string name, TimeSpan interval, Func<CancellationToken, ValueTask> tickAsync, ILogger logger)
		{
			async ValueTask<TimeSpan?> WrappedTrigger(CancellationToken token)
			{
				Stopwatch timer = Stopwatch.StartNew();
				await tickAsync(token);
				return interval - timer.Elapsed;
			}

			return clock.AddTicker(name, interval, WrappedTrigger, logger);
		}

		/// <summary>
		/// Create an event that will trigger after the given time
		/// </summary>
		/// <param name="clock">Clock to schedule the event on</param>
		/// <param name="interval">Time after which the event will trigger</param>
		/// <param name="tickAsync">Callback for the tick. Returns the time interval until the next tick, or null to cancel the tick.</param>
		/// <param name="logger">Logger for error messages</param>
		/// <returns>Handle to the event</returns>
		public static ITicker AddTicker<T>(this IClock clock, TimeSpan interval, Func<CancellationToken, ValueTask<TimeSpan?>> tickAsync, ILogger logger) => clock.AddTicker(typeof(T).Name, interval, tickAsync, logger);

		/// <summary>
		/// Create an event that will trigger after the given time
		/// </summary>
		/// <param name="clock">Clock to schedule the event on</param>
		/// <param name="interval">Interval for the callback</param>
		/// <param name="tickAsync">Trigger callback</param>
		/// <param name="logger">Logger for any error messages</param>
		/// <returns>Handle to the event</returns>
		public static ITicker AddTicker<T>(this IClock clock, TimeSpan interval, Func<CancellationToken, ValueTask> tickAsync, ILogger logger) => clock.AddTicker(typeof(T).Name, interval, tickAsync, logger);

		/// <summary>
		/// Create a ticker shared between all server pods
		/// </summary>
		/// <param name="clock">Clock to schedule the event on</param>
		/// <param name="interval">Time after which the event will trigger</param>
		/// <param name="tickAsync">Callback for the tick. Returns the time interval until the next tick, or null to cancel the tick.</param>
		/// <param name="logger">Logger for error messages</param>
		/// <returns>New ticker instance</returns>
		public static ITicker AddSharedTicker<T>(this IClock clock, TimeSpan interval, Func<CancellationToken, ValueTask> tickAsync, ILogger logger) => clock.AddSharedTicker(typeof(T).Name, interval, tickAsync, logger);
	}

	/// <summary>
	/// Implementation of <see cref="IClock"/> which returns the current time
	/// </summary>
	public class Clock : IClock
	{
		sealed class TickerImpl : ITicker, IDisposable
		{
			readonly string _name;
			readonly CancellationTokenSource _cancellationSource;
			readonly Func<Task> _tickFunc;
			Task? _backgroundTask;

			public TickerImpl(string name, TimeSpan delay, Func<CancellationToken, ValueTask<TimeSpan?>> triggerAsync, ILogger logger)
			{
				_name = name;
				_cancellationSource = new CancellationTokenSource();
				_tickFunc = () => Run(delay, triggerAsync, logger);
			}

			public async Task StartAsync()
			{
				await StopAsync();

				_backgroundTask = Task.Run(_tickFunc);
			}

			public async Task StopAsync()
			{
				if (_backgroundTask != null)
				{
					_cancellationSource.Cancel();
					await _backgroundTask;
				}
			}

			public void Dispose()
			{
				_cancellationSource.Dispose();
			}

			public async Task Run(TimeSpan delay, Func<CancellationToken, ValueTask<TimeSpan?>> triggerAsync, ILogger logger)
			{
				while (!_cancellationSource!.IsCancellationRequested)
				{
					try
					{
						if (delay > TimeSpan.Zero)
						{
							await Task.Delay(delay, _cancellationSource.Token);
						}

						TimeSpan? nextDelay = await triggerAsync(_cancellationSource.Token);
						if(nextDelay == null)
						{
							break;
						}

						delay = nextDelay.Value;
					}
					catch (OperationCanceledException) when (_cancellationSource.IsCancellationRequested)
					{
					}
					catch (Exception ex)
					{
						logger.LogError(ex, "Exception while executing scheduled event");
						if (delay < TimeSpan.Zero)
						{
							delay = TimeSpan.FromSeconds(5.0);
							logger.LogWarning("Delaying tick for 5 seconds");
						}
					}
				}
			}

			public override string ToString() => _name;
		}

		readonly RedisService _redis;

		/// <inheritdoc/>
		public DateTime UtcNow => DateTime.UtcNow;

		/// <summary>
		/// Constructor
		/// </summary>
		public Clock(RedisService redis)
		{
			_redis = redis;
		}

		/// <inheritdoc/>
		public ITicker AddTicker(string name, TimeSpan delay, Func<CancellationToken, ValueTask<TimeSpan?>> tickAsync, ILogger logger)
		{
			return new TickerImpl(name, delay, tickAsync, logger);
		}

		/// <inheritdoc/>
		public ITicker AddSharedTicker(string name, TimeSpan delay, Func<CancellationToken, ValueTask> tickAsync, ILogger logger)
		{
			RedisKey key = new RedisKey($"tick/{name}");
			return ClockExtensions.AddTicker(this, name, delay / 4, token => TriggerSharedAsync(key, delay, tickAsync, token), logger);
		}

		async ValueTask TriggerSharedAsync(RedisKey key, TimeSpan interval, Func<CancellationToken, ValueTask> tickAsync, CancellationToken cancellationToken)
		{
			using (RedisLock sharedLock = new (_redis.GetDatabase(), key))
			{
				if (await sharedLock.AcquireAsync(interval, false))
				{
					await tickAsync(cancellationToken);
				}
			}
		}
	}

	/// <summary>
	/// Fake clock that doesn't advance by wall block time
	/// Requires manual ticking to progress. Used in tests.
	/// </summary>
	public class FakeClock : IClock
	{
		class TickerImpl : ITicker
		{
			readonly FakeClock _outer;
			readonly string _name;
			readonly TimeSpan _interval;
			public DateTime? NextTime { get; set; }
			public Func<CancellationToken, ValueTask<TimeSpan?>> TickAsync { get; }

			public TickerImpl(FakeClock outer, string name, TimeSpan interval, Func<CancellationToken, ValueTask<TimeSpan?>> tickAsync)
			{
				_outer = outer;
				_name = name;
				_interval = interval;
				TickAsync = tickAsync;

				lock (outer._triggers)
				{
					outer._triggers.Add(this);
				}
			}

			public Task StartAsync()
			{
				NextTime = _outer.UtcNow + _interval;
				return Task.CompletedTask;
			}

			public Task StopAsync()
			{
				NextTime = null;
				return Task.CompletedTask;
			}

			public void Dispose()
			{
				DisposeAsync().AsTask().Wait();
			}

			public ValueTask DisposeAsync()
			{
				lock (_outer._triggers)
				{
					_outer._triggers.Remove(this);
				}
				return new ValueTask();
			}

			public override string ToString()
			{
				if (NextTime == null)
				{
					return $"{_name} (paused)";
				}
				else
				{
					return $"{_name} ({NextTime.Value})";
				}
			}
		}

		DateTime _utcNowPrivate;
		readonly List<TickerImpl> _triggers = new List<TickerImpl>();

		/// <summary>
		/// Constructor
		/// </summary>
		public FakeClock()
		{
			_utcNowPrivate = DateTime.UtcNow;
		}

		/// <summary>
		/// Advance time by given amount
		/// Useful for letting time progress during tests
		/// </summary>
		/// <param name="period">Time span to advance</param>
		public async Task AdvanceAsync(TimeSpan period)
		{
			_utcNowPrivate = _utcNowPrivate.Add(period);

			for (int idx = 0; idx < _triggers.Count; idx++)
			{
				TickerImpl trigger = _triggers[idx];
				while (trigger.NextTime != null && _utcNowPrivate > trigger.NextTime)
				{
					TimeSpan? delay = await trigger.TickAsync(CancellationToken.None);
					if (delay == null)
					{
						_triggers.RemoveAt(idx--);
						break;
					}
					trigger.NextTime = _utcNowPrivate + delay.Value;
				}
			}
		}

		/// <inheritdoc/>
		public DateTime UtcNow
		{ 
			get => _utcNowPrivate;
			set => _utcNowPrivate = value.ToUniversalTime(); 
		}

		/// <inheritdoc/>
		public ITicker AddTicker(string name, TimeSpan interval, Func<CancellationToken, ValueTask<TimeSpan?>> tickAsync, ILogger logger)
		{
			return new TickerImpl(this, name, interval, tickAsync);
		}

		/// <inheritdoc/>
		public ITicker AddSharedTicker(string name, TimeSpan interval, Func<CancellationToken, ValueTask> tickAsync, ILogger logger)
		{
			async ValueTask<TimeSpan?> Tick(CancellationToken token)
			{
				await tickAsync(token);
				return interval;
			}
			return new TickerImpl(this, name, interval, Tick);
		}
	}
}