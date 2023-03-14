// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;

namespace Horde.Build.Utilities
{
	/// <summary>
	/// Runs a task in the background and allows stopping it on demand
	/// </summary>
	public sealed class BackgroundTask : IDisposable
	{
		readonly Func<CancellationToken, Task> _runTask;
		Task _task = Task.CompletedTask;
		readonly CancellationTokenSource _cancellationTokenSource = new CancellationTokenSource();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="runTask"></param>
		public BackgroundTask(Func<CancellationToken, Task> runTask)
		{
			_runTask = runTask;
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			if (!_task.IsCompleted)
			{
				_cancellationTokenSource.Cancel();
				StopAsync().Wait();
			}
			_cancellationTokenSource.Dispose();
		}

		/// <summary>
		/// Starts the task
		/// </summary>
		public void Start()
		{
			if (!_task.IsCompleted)
			{
				throw new InvalidOperationException("Background task is already running");
			}
			_task = Task.Run(() => _runTask(_cancellationTokenSource.Token), _cancellationTokenSource.Token);
		}

		/// <summary>
		/// Signals the cancellation token and waits for the task to finish
		/// </summary>
		/// <returns></returns>
		public async Task StopAsync()
		{
			try
			{
				_cancellationTokenSource.Cancel();
				await _task;
			}
			catch (OperationCanceledException)
			{
			}
		}
	}
}
