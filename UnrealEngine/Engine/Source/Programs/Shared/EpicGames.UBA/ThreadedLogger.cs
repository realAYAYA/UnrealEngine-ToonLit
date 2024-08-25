// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Logging;
using System.Collections.Generic;
using System.Threading.Tasks;
using System.Threading;
using System;

namespace EpicGames.UBA
{
	/// <summary>
	/// Threaded logging for use by UBAExecutor
	/// </summary>
	public class ThreadedLogger : Microsoft.Extensions.Logging.ILogger, IDisposable
	{
		interface ILogEntry
		{
			void Log();
		}

		readonly Microsoft.Extensions.Logging.ILogger _internalLogger;
		readonly Task _logTask;
		readonly object _logQueueLock;
		readonly CancellationToken _token;
		readonly EventWaitHandle _processLogQueue;

		List<Action> _logQueue;
		long _running;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="logger">The logger</param>
		public ThreadedLogger(Microsoft.Extensions.Logging.ILogger logger)
		{
			_internalLogger = logger;

			_running = 1;
			_logQueueLock = new object();
			_processLogQueue = new EventWaitHandle(false, EventResetMode.AutoReset);
			_logQueue = new List<Action>();
			_token = new CancellationToken();
			_logTask = Task.Factory.StartNew(() =>
			{
				List<Action> logQueue2 = new();
				while (Interlocked.Read(ref _running) == 1)
				{
					_processLogQueue.WaitOne();
					lock (_logQueueLock)
					{
						(logQueue2, _logQueue) = (_logQueue, logQueue2);
					}

					foreach (Action entry in logQueue2)
					{
						entry();
					}
					logQueue2.Clear();
				}
			}, _token, TaskCreationOptions.LongRunning, TaskScheduler.Default);
		}

		#region IDisposable
		/// <summary>
		/// Destructor
		/// </summary>
		~ThreadedLogger() => Dispose(false);

		/// <inheritdoc/>
		public void Dispose()
		{
			Dispose(true);
			GC.SuppressFinalize(this);
		}

		/// <summary>
		/// Protected dispose
		/// </summary>
		protected virtual void Dispose(bool disposing)
		{
			if (disposing)
			{
				_processLogQueue.Dispose();
			}
		}
		#endregion

		#region Microsoft.Extensions.Logging.ILogger
		/// <inheritdoc/>
		public void Log<TState>(LogLevel logLevel, EventId eventId, TState state, Exception? exception, Func<TState, Exception?, string> formatter)
		{
			if (logLevel < LogLevel.Error)
			{
				lock (_logQueueLock)
				{
					if (_running == 0)
					{
						_internalLogger.Log(logLevel, eventId, state, exception, formatter);
						return;
					}
					_logQueue.Add(() => _internalLogger.Log(logLevel, eventId, state, exception, formatter));
				}
				_processLogQueue.Set();
			}
			else
			{
				using EventWaitHandle waitForLog = new(false, EventResetMode.ManualReset);
				lock (_logQueueLock)
				{
					if (_running == 0)
					{
						_internalLogger.Log(logLevel, eventId, state, exception, formatter);
						return;
					}
					_logQueue.Add(() =>
					{
						_internalLogger.Log(logLevel, eventId, state, exception, formatter);
						waitForLog.Set();
					});
				}
				_processLogQueue.Set();
				waitForLog.WaitOne();
			}
		}

		/// <inheritdoc/>
		public bool IsEnabled(LogLevel logLevel)
		{
			return _internalLogger.IsEnabled(logLevel);
		}

		/// <inheritdoc/>
		public IDisposable BeginScope<TState>(TState state)
		{
			return _internalLogger.BeginScope(state);
		}
		#endregion

		/// <summary>
		/// Finish logging async
		/// </summary>
		public async Task FinishAsync()
		{
			lock (_logQueueLock)
			{
				Interlocked.Exchange(ref _running, 0);
				_processLogQueue.Set();
			}

			await _logTask.WaitAsync(_token);
		}
	}
}