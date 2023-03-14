// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;

namespace Horde.Build.Utilities
{
	/// <summary>
	/// Extension method to wrap a semaphore in a using statement
	/// </summary>
	public static class SemaphoreSlimExtensions
	{
		/// <summary>
		/// Returns a disposable semaphore
		/// </summary>
		/// <param name="semaphore">the semaphore to wrap</param>
		/// <param name="cancelToken">optional cancellation token</param>
		/// <returns></returns>
		public static async Task<IDisposable> UseWaitAsync(this SemaphoreSlim semaphore, CancellationToken cancelToken = default)
		{
			await semaphore.WaitAsync(cancelToken).ConfigureAwait(false);
			return new ReleaseWrapper(semaphore);
		}

		/// <summary>
		/// Semaphore wrapper implementing IDisposable
		/// </summary>
		private class ReleaseWrapper : IDisposable
		{
			/// <summary>
			/// The semaphore to wrap
			/// </summary>
			private readonly SemaphoreSlim _semaphore;

			/// <summary>
			/// Whether this has been disposed
			/// </summary>
			private bool _bIsDisposed;

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="semaphore">The semaphore to wrap</param>
			public ReleaseWrapper(SemaphoreSlim semaphore)
			{
				_semaphore = semaphore;
			}

			/// <summary>
			/// Releases the lock on dispose
			/// </summary>
			public void Dispose()
			{
				if (_bIsDisposed)
				{
					return;
				}

				_semaphore.Release();
				_bIsDisposed = true;
			}
		}
	}
}
