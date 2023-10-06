// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using EpicGames.Core;

namespace UnrealBuildTool
{
	/// <summary>
	/// System-wide mutex allowing only one instance of the program to run at a time
	/// </summary>
	class SingleInstanceMutex : IDisposable
	{
		/// <summary>
		/// The global mutex instance
		/// </summary>
		Mutex? GlobalMutex;

		/// <summary>
		/// Constructor. Attempts to acquire the global mutex
		/// </summary>
		/// <param name="MutexName">Name of the mutex to acquire</param>
		/// <param name="bWaitMutex"></param>
		public SingleInstanceMutex(string MutexName, bool bWaitMutex)
		{
			// Try to create the mutex, with it initially locked
			bool bCreatedMutex;
			GlobalMutex = new Mutex(true, MutexName, out bCreatedMutex);

			// If we didn't create the mutex, we can wait for it or fail immediately
			if (!bCreatedMutex)
			{
				if (bWaitMutex)
				{
					try
					{
						GlobalMutex.WaitOne();
					}
					catch (AbandonedMutexException)
					{
					}
				}
				else
				{
					throw new BuildException("A conflicting instance of UnrealBuildTool is already running.");
				}
			}
		}

		/// <summary>
		/// Gets the name of a mutex unique for the given path
		/// </summary>
		/// <param name="Name">Base name of the mutex</param>
		/// <param name="UniquePath">Path to identify a unique mutex</param>
		public static string GetUniqueMutexForPath(string Name, string UniquePath)
		{
			// generate a md5 hash of the path, as GetHashCode is not guaranteed to generate a stable hash
			return String.Format("Global\\{0}_{1}", Name, ContentHash.MD5(UniquePath.ToUpperInvariant()));
		}

		/// <summary>
		/// Release the mutex and dispose of the object
		/// </summary>
		public void Dispose()
		{
			if (GlobalMutex != null)
			{
				GlobalMutex.ReleaseMutex();
				GlobalMutex.Dispose();
				GlobalMutex = null;
			}
		}
	}
}
