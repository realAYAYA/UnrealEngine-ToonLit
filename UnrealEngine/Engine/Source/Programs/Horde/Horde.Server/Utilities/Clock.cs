// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Redis.Utility;
using Horde.Server;
using Horde.Server.Server;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using OpenTelemetry.Trace;
using StackExchange.Redis;
using TimeZoneConverter;

namespace HordeCommon
{
	/// <summary>
	/// Base interface for a scheduled event
	/// </summary>
	public interface ITicker : IAsyncDisposable
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
		public ValueTask DisposeAsync() => new ValueTask();

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
		DateTime UtcNow { get; }

		/// <summary>
		/// Time zone for schedules etc...
		/// </summary>
		TimeZoneInfo TimeZone { get; }

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
		/// Create a ticker shared between all server processes.
		/// Callback can be run inside any available process but will still only be called once per tick.
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

		/// <summary>
		/// Gets the start of the day for the given datetime in UTC, respecting the configured timezone.
		/// </summary>
		/// <param name="timeZone">Time zone to adjust for</param>
		/// <param name="time">Time to convert</param>
		/// <returns>UTC datetime for the start of the day</returns>
		public static DateTime GetStartOfDayUtc(this TimeZoneInfo timeZone, DateTime time)
		{
			DateTime currentTimeLocal = TimeZoneInfo.ConvertTime(time, timeZone);
			DateTime startOfDayLocal = currentTimeLocal - currentTimeLocal.TimeOfDay;
			return TimeZoneInfo.ConvertTime(startOfDayLocal, timeZone, TimeZoneInfo.Utc);
		}
	}

	/// <summary>
	/// Implementation of <see cref="IClock"/> which returns the current time
	/// </summary>
	public sealed class Clock : IClock, IAsyncDisposable
	{
		sealed class TickerImpl : ITicker, IAsyncDisposable
		{
			readonly string _name;
			readonly CancellationTokenSource _cancellationSource;
			readonly Func<Task> _tickFunc;
			Task? _backgroundTask;

			public TickerImpl(string name, TimeSpan delay, Func<CancellationToken, ValueTask<TimeSpan?>> triggerAsync, ILogger logger)
			{
				_name = name;
				_cancellationSource = new CancellationTokenSource();
				_tickFunc = () => RunAsync(delay, triggerAsync, logger);
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
					await _cancellationSource.CancelAsync();
					await _backgroundTask;
					_backgroundTask = null;
				}
			}

			public async ValueTask DisposeAsync()
			{
				await StopAsync();
				_cancellationSource.Dispose();
			}

			public async Task RunAsync(TimeSpan delay, Func<CancellationToken, ValueTask<TimeSpan?>> triggerAsync, ILogger logger)
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
						if (nextDelay == null)
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
		readonly Tracer _tracer;
		readonly TimeZoneInfo _timeZone;
		readonly List<TickerImpl> _tickers = new List<TickerImpl>();

		/// <inheritdoc/>
		public DateTime UtcNow => DateTime.UtcNow;

		/// <inheritdoc/>
		public TimeZoneInfo TimeZone => _timeZone;

		/// <summary>
		/// Constructor
		/// </summary>
		public Clock(RedisService redis, Tracer tracer, IOptions<ServerSettings> settings)
		{
			_redis = redis;
			_tracer = tracer;

			string? timeZoneName = settings.Value.ScheduleTimeZone;
			_timeZone = (timeZoneName == null) ? TimeZoneInfo.Local : TZConvert.GetTimeZoneInfo(timeZoneName);
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			foreach (TickerImpl ticker in _tickers)
			{
				await ticker.DisposeAsync();
			}
		}

		/// <inheritdoc/>
		public ITicker AddTicker(string name, TimeSpan delay, Func<CancellationToken, ValueTask<TimeSpan?>> tickAsync, ILogger logger)
		{
			TickerImpl ticker = new TickerImpl(name, delay, tickAsync, logger);
			lock (_tickers)
			{
				_tickers.Add(ticker);
			}
			return ticker;
		}

		/// <inheritdoc/>
		public ITicker AddSharedTicker(string name, TimeSpan delay, Func<CancellationToken, ValueTask> tickAsync, ILogger logger)
		{
			RedisKey key = new RedisKey($"tick/{name}");
			return ClockExtensions.AddTicker(this, name, delay / 4, token => TriggerSharedAsync(name, key, delay, tickAsync, token), logger);
		}

		async ValueTask TriggerSharedAsync(string name, RedisKey key, TimeSpan interval, Func<CancellationToken, ValueTask> tickAsync, CancellationToken cancellationToken)
		{
			using (RedisLock sharedLock = new(_redis.GetDatabase(), key))
			{
				if (await sharedLock.AcquireAsync(interval, false))
				{
					using TelemetrySpan span = _tracer.StartActiveSpan(name);
					span.SetAttribute("interval", interval.TotalSeconds);

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
			TimeZone = TimeZoneInfo.Utc;
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
		public TimeZoneInfo TimeZone { get; set; }

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