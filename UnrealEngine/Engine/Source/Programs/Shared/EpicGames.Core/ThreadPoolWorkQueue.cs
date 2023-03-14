// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;

namespace EpicGames.Core
{
	/// <summary>
	/// Allows queuing a large number of tasks to a thread pool and waiting for them all to complete.
	/// </summary>
	public class ThreadPoolWorkQueue : IDisposable
	{
		/// <summary>
		/// Object used for controlling access to NumOutstandingJobs and updating EmptyEvent
		/// </summary>
		readonly object _lockObject = new object();

		/// <summary>
		/// Number of jobs remaining in the queue. This is updated in an atomic way.
		/// </summary>
		int _numOutstandingJobs;

		/// <summary>
		/// Event which indicates whether the queue is empty.
		/// </summary>
		ManualResetEvent _emptyEvent = new ManualResetEvent(true);

		/// <summary>
		/// Exceptions which occurred while executing tasks
		/// </summary>
		readonly List<Exception> _exceptions = new List<Exception>();

		/// <summary>
		/// Default constructor
		/// </summary>
		public ThreadPoolWorkQueue()
		{
		}

		/// <summary>
		/// Waits for the queue to drain, and disposes of it
		/// </summary>
		public void Dispose()
		{
			if(_emptyEvent != null)
			{
				Wait();

				_emptyEvent.Dispose();
				_emptyEvent = null!;
			}
		}

		/// <summary>
		/// Returns the number of items remaining in the queue
		/// </summary>
		public int NumRemaining => _numOutstandingJobs;

		/// <summary>
		/// Adds an item to the queue
		/// </summary>
		/// <param name="actionToExecute">The action to add</param>
		public void Enqueue(Action actionToExecute)
		{
			lock(_lockObject)
			{
				if(_numOutstandingJobs == 0)
				{
					_emptyEvent.Reset();
				}
				_numOutstandingJobs++;
			}

#if SINGLE_THREAD
			Execute(ActionToExecute);
#else
			ThreadPool.QueueUserWorkItem(Execute, actionToExecute);
#endif
		}

		/// <summary>
		/// Internal method to execute an action
		/// </summary>
		/// <param name="actionToExecute">The action to execute</param>
		void Execute(object? actionToExecute)
		{
			try
			{
				((Action)actionToExecute!)();
			}
			catch(Exception ex)
			{
				lock(_lockObject)
				{
					_exceptions.Add(ex);
				}
			}
			finally
			{
				lock(_lockObject)
				{
					_numOutstandingJobs--;
					if(_numOutstandingJobs == 0)
					{
						_emptyEvent.Set();
					}
				}
			}
		}

		/// <summary>
		/// Waits for all queued tasks to finish
		/// </summary>
		public void Wait()
		{
			_emptyEvent.WaitOne();
			RethrowExceptions();
		}

		/// <summary>
		/// Waits for all queued tasks to finish, or the timeout to elapse
		/// </summary>
		/// <param name="millisecondsTimeout">Maximum time to wait</param>
		/// <returns>True if the queue completed, false if the timeout elapsed</returns>
		public bool Wait(int millisecondsTimeout)
		{
			return Wait(TimeSpan.FromMilliseconds(millisecondsTimeout));
		}

		/// <summary>
		/// Waits for all queued tasks to finish, or the timeout to elapse
		/// </summary>
		/// <param name="timeout">Maximum time to wait</param>
		/// <returns>True if the queue completed, false if the timeout elapsed</returns>
		public bool Wait(TimeSpan timeout)
		{
			bool bResult = _emptyEvent.WaitOne(timeout);
			if (bResult)
			{
				RethrowExceptions();
			}
			return bResult;
		}

		/// <summary>
		/// Checks for any exceptions which ocurred in queued tasks, and re-throws them on the current thread
		/// </summary>
		public void RethrowExceptions()
		{
			lock(_lockObject)
			{
				if(_exceptions.Count > 0)
				{
					throw new AggregateException(_exceptions.ToArray());
				}
			}
		}
	}
}
