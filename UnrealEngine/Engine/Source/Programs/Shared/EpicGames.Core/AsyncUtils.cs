// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Core
{
	/// <summary>
	/// Utility functions for manipulating async tasks
	/// </summary>
	public static class AsyncUtils
	{
		/// <summary>
		/// Converts a cancellation token to a waitable task
		/// </summary>
		/// <param name="token">Cancellation token</param>
		/// <returns></returns>
		public static Task AsTask(this CancellationToken token)
		{
			return Task.Delay(-1, token).ContinueWith(x => { });
		}

		/// <summary>
		/// Converts a cancellation token to a waitable task
		/// </summary>
		/// <param name="token">Cancellation token</param>
		/// <returns></returns>
		public static Task<T> AsTask<T>(this CancellationToken token)
		{
			return Task.Delay(-1, token).ContinueWith(_ => Task.FromCanceled<T>(token)).Unwrap();
		}

		/// <summary>
		/// Attempts to get the result of a task, if it has finished
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="task"></param>
		/// <param name="result"></param>
		/// <returns></returns>
		public static bool TryGetResult<T>(this Task<T> task, out T result)
		{
			if (task.IsCompleted)
			{
				result = task.Result;
				return true;
			}
			else
			{
				result = default!;
				return false;
			}
		}

		/// <summary>
		/// Waits for a time period to elapse or the task to be cancelled, without throwing an cancellation exception
		/// </summary>
		/// <param name="time">Time to wait</param>
		/// <param name="token">Cancellation token</param>
		/// <returns></returns>
		public static Task DelayNoThrow(TimeSpan time, CancellationToken token)
		{
			return Task.Delay(time, token).ContinueWith(x => { });
		}

		/// <summary>
		/// Removes all the complete tasks from a list, allowing each to throw exceptions as necessary
		/// </summary>
		/// <param name="tasks">List of tasks to remove tasks from</param>
		public static void RemoveCompleteTasks(this List<Task> tasks)
		{
			List<Exception> exceptions = new List<Exception>();

			int outIdx = 0;
			for (int idx = 0; idx < tasks.Count; idx++)
			{
				if (tasks[idx].IsCompleted)
				{
					AggregateException? exception = tasks[idx].Exception;
					if (exception != null)
					{
						exceptions.AddRange(exception.InnerExceptions);
					}
				}
				else
				{
					if (idx != outIdx)
					{
						tasks[outIdx] = tasks[idx];
					}
					outIdx++;
				}
			}
			tasks.RemoveRange(outIdx, tasks.Count - outIdx);

			if (exceptions.Count > 0)
			{
				throw new AggregateException(exceptions);
			}
		}

		/// <summary>
		/// Removes all the complete tasks from a list, allowing each to throw exceptions as necessary
		/// </summary>
		/// <param name="tasks">List of tasks to remove tasks from</param>
		/// <returns>Return values from the completed tasks</returns>
		public static List<T> RemoveCompleteTasks<T>(this List<Task<T>> tasks)
		{
			List<T> results = new List<T>();

			int outIdx = 0;
			for (int idx = 0; idx < tasks.Count; idx++)
			{
				if (tasks[idx].IsCompleted)
				{
					results.Add(tasks[idx].Result);
				}
				else if (idx != outIdx)
				{
					tasks[outIdx++] = tasks[idx];
				}
			}
			tasks.RemoveRange(outIdx, tasks.Count - outIdx);

			return results;
		}
	}
}
