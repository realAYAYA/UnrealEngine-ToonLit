// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Core
{
	/// <summary>
	/// Allows taking a lock on a global mutex from async code.
	/// </summary>
	public static class SingleInstanceMutex
	{
		class SingleInstanceMutexImpl : IDisposable
		{
			readonly string? _name;
			readonly TaskCompletionSource _readyTcs = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
			readonly ManualResetEvent _disposing = new ManualResetEvent(false);
			Thread? _thread;

			public SingleInstanceMutexImpl(string name)
			{
				_name = name;

				_thread = new Thread(() => ThreadFunc());
				_thread.Start();
			}

			public async ValueTask WaitAsync(CancellationToken cancellationToken)
			{
				using (CancellationTokenRegistration registration = cancellationToken.Register(() => _readyTcs.TrySetCanceled()))
				{
					await _readyTcs.Task;
				}
			}

			public void Dispose()
			{
				if (_thread != null)
				{
					_disposing.Set();

					_thread.Join();
					_thread = null;
				}

				_disposing.Dispose();
			}

			void ThreadFunc()
			{
				using Mutex mutex = new Mutex(false, _name);
				try
				{
					// Waiting for multiple handles is only supported on Windows
					if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
					{
						WaitHandle[] handles = new WaitHandle[2];
						handles[0] = _disposing;
						handles[1] = mutex;

						if (WaitHandle.WaitAny(handles) == 0)
						{
							return;
						}
					}
					else
					{
						while(!mutex.WaitOne(100))
						{
							if(_disposing.WaitOne(0))
							{
								return;
							}
						}
					}
				}
				catch (AbandonedMutexException)
				{
				}

				_readyTcs.TrySetResult();
				_disposing.WaitOne();

				mutex.ReleaseMutex();
			}
		}

		/// <summary>
		/// Acquires a global mutex with the given name.
		/// </summary>
		/// <param name="name">Name of the mutex to acquire</param>
		/// <param name="cancellationToken">Cancellation token for the wait</param>
		/// <returns>Handle to the mutex. Must be disposed when complete.</returns>
		public static async Task<IDisposable> AcquireAsync(string name, CancellationToken cancellationToken = default)
		{
			SingleInstanceMutexImpl? impl = new SingleInstanceMutexImpl(name);
			try
			{
				await impl.WaitAsync(cancellationToken);
				cancellationToken.ThrowIfCancellationRequested();

				SingleInstanceMutexImpl result = impl;
				impl = null;
				return result;
			}
			finally
			{
#pragma warning disable CA1508 // Avoid dead conditional code
				impl?.Dispose();
#pragma warning restore CA1508 // Avoid dead conditional code
			}
		}
	}
}
