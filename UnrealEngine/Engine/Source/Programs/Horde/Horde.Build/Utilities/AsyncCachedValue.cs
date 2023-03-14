// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.Threading;
using System.Threading.Tasks;

namespace Horde.Build.Utilities
{
	/// <summary>
	/// Caches a value and asynchronously updates it after a period of time
	/// </summary>
	/// <typeparam name="T"></typeparam>
	public class AsyncCachedValue<T>
	{
		class State
		{
			public readonly T Value;
			readonly Stopwatch _timer;
			public Task<State>? _next;

			public TimeSpan Elapsed => _timer.Elapsed;

			public State(T value)
			{
				Value = value;
				_timer = Stopwatch.StartNew();
			}
		}

		/// <summary>
		/// The current state
		/// </summary>
		Task<State>? _current = null;

		/// <summary>
		/// Generator for the new value
		/// </summary>
		readonly Func<Task<T>> _generator;

		/// <summary>
		/// Time at which to start to refresh the value
		/// </summary>
		readonly TimeSpan _minRefreshTime;

		/// <summary>
		/// Time at which to wait for the value to refresh
		/// </summary>
		readonly TimeSpan _maxRefreshTime;

		/// <summary>
		/// Default constructor
		/// </summary>
		public AsyncCachedValue(Func<Task<T>> generator, TimeSpan refreshTime)
			: this(generator, refreshTime * 0.75, refreshTime)
		{
		}

		/// <summary>
		/// Default constructor
		/// </summary>
		public AsyncCachedValue(Func<Task<T>> generator, TimeSpan minRefreshTime, TimeSpan maxRefreshTime)
		{
			_generator = generator;
			_minRefreshTime = minRefreshTime;
			_maxRefreshTime = maxRefreshTime;
		}

		/// <summary>
		/// Tries to get the current value
		/// </summary>
		/// <returns>The cached value, if valid</returns>
		public Task<T> GetAsync()
		{
			return GetAsync(_maxRefreshTime);
		}

		Task<State> CreateOrGetStateTask(ref Task<State>? stateTask)
		{
			for(; ;)
			{
				Task<State>? currentStateTask = stateTask;
				if (currentStateTask != null)
				{
					return currentStateTask;
				}

				Task<Task<State>> innerNewStateTask = new Task<Task<State>>(() => CreateState());
				if (Interlocked.CompareExchange(ref stateTask, innerNewStateTask.Unwrap(), null) == null)
				{
					innerNewStateTask.Start();
				}
			}
		}

		async Task<State> CreateState()
		{
			return new State(await _generator());
		}

		/// <summary>
		/// Tries to get the current value
		/// </summary>
		/// <returns>The cached value, if valid</returns>
		public async Task<T> GetAsync(TimeSpan maxAge)
		{
			Task<State> stateTask = CreateOrGetStateTask(ref _current);

			State state = await stateTask;
			if (state._next != null && state._next.IsCompleted)
			{
				_ = Interlocked.CompareExchange(ref _current, state._next, stateTask);
				state = await state._next;
			}
			if (state.Elapsed > maxAge)
			{
				state = await CreateOrGetStateTask(ref state._next);
			}
			if (state.Elapsed > _minRefreshTime)
			{
				_ = CreateOrGetStateTask(ref state._next);
			}

			return state.Value;
		}
	}
}
