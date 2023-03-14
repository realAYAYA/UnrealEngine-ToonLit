// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using Microsoft.Extensions.Logging;
using Microsoft.Win32.SafeHandles;

namespace EpicGames.Core
{
	/// <summary>
	/// Includes utilities for managing processes
	/// </summary>
	public static class ProcessUtils
	{
		const uint PROCESS_TERMINATE = 0x0001;
		const uint PROCESS_QUERY_LIMITED_INFORMATION = 0x1000;

		[DllImport("kernel32.dll", SetLastError = true)]
		static extern SafeProcessHandle OpenProcess(uint dwDesiredAccess, bool bInheritHandle, uint processId);

		[DllImport("kernel32.dll", SetLastError = true)]
		[return: MarshalAs(UnmanagedType.Bool)]
		static extern bool TerminateProcess(SafeProcessHandle hProcess, uint uExitCode);

		[DllImport("kernel32.dll", SetLastError = true)]
		static extern int QueryFullProcessImageName([In]SafeProcessHandle hProcess, [In]int dwFlags, [Out]StringBuilder lpExeName, ref int lpdwSize);

		[DllImport("Psapi.dll", SetLastError = true)]
		static extern bool EnumProcesses([MarshalAs(UnmanagedType.LPArray, ArraySubType = UnmanagedType.U4)] [In][Out] uint[] processIds, int arraySizeBytes, [MarshalAs(UnmanagedType.U4)] out int bytesCopied);

		[DllImport("kernel32.dll")]
		static extern uint WaitForMultipleObjects(int nCount, IntPtr[] lpHandles, bool bWaitAll, uint dwMilliseconds);

		/// <summary>
		/// Attempts to terminate all processes matching a predicate
		/// </summary>
		/// <param name="predicate">The predicate for whether to terminate a process</param>
		/// <param name="logger">Logging device</param>
		/// <param name="cancellationToken">Cancellation token to abort the search</param>
		/// <returns>True if the process succeeded</returns>
		public static bool TerminateProcesses(Predicate<FileReference> predicate, ILogger logger, CancellationToken cancellationToken)
		{
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				return TerminateProcessesWin32(predicate, logger, cancellationToken);
			}
			else
			{
				return TerminateProcessesGenericPlatform(predicate, logger, cancellationToken);
			}
		}

		/// <summary>
		/// Attempts to terminate all processes matching a predicate
		/// </summary>
		/// <param name="predicate">The predicate for whether to terminate a process</param>
		/// <param name="logger">Logging device</param>
		/// <param name="cancellationToken">Cancellation token to abort the search</param>
		/// <returns>True if the process succeeded</returns>
		static bool TerminateProcessesWin32(Predicate<FileReference> predicate, ILogger logger, CancellationToken cancellationToken)
		{
			Dictionary<int, int> processToCount = new Dictionary<int, int>();
			for(; ;)
			{
				cancellationToken.ThrowIfCancellationRequested();

				// Enumerate the processes
				int numProcessIds;
				uint[] processIds = new uint[512];
				for (; ; )
				{
					int maxBytes = processIds.Length * sizeof(uint);
					int numBytes = 0;
					if (!EnumProcesses(processIds, maxBytes, out numBytes))
					{
						throw new Win32ExceptionWithCode("Unable to enumerate processes");
					}
					if (numBytes < maxBytes)
					{
						numProcessIds = numBytes / sizeof(uint);
						break;
					}
					processIds = new uint[processIds.Length + 256];
				}

				// Find the processes to terminate
				List<SafeProcessHandle> waitHandles = new List<SafeProcessHandle>();
				try
				{
					// Open each process in turn
					foreach (uint processId in processIds)
					{
						SafeProcessHandle handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, false, processId);
						try
						{
							if (!handle.IsInvalid)
							{
								int characters = 260;
								StringBuilder buffer = new StringBuilder(characters);
								if (QueryFullProcessImageName(handle, 0, buffer, ref characters) > 0)
								{
									FileReference imageFile = new FileReference(buffer.ToString(0, characters));
									if (predicate(imageFile))
									{
										logger.LogInformation("Terminating {ImageName} ({ProcessId})", imageFile, processId);
										if (TerminateProcess(handle, 9))
										{
											waitHandles.Add(handle);
										}
										else
										{
											logger.LogInformation("Failed call to TerminateProcess ({Code})", Marshal.GetLastWin32Error());
										}
										handle.SetHandleAsInvalid();
									}
								}
							}
						}
						finally
						{
							handle.Dispose();
						}
					}

					// If there's nothing to do, exit immediately
					if (waitHandles.Count == 0)
					{
						return true;
					}

					// Wait for them all to complete
					WaitForMultipleObjects(waitHandles.Count, waitHandles.Select(x => x.DangerousGetHandle()).ToArray(), true, 10 * 1000);
				}
				finally
				{
					foreach (SafeProcessHandle waitHandle in waitHandles)
					{
						waitHandle.Close();
					}
				}
			}
		}

		/// <summary>
		/// Attempts to terminate all processes matching a predicate
		/// </summary>
		/// <param name="predicate">The predicate for whether to terminate a process</param>
		/// <param name="logger">Logging device</param>
		/// <param name="cancellationToken">Cancellation token to abort the search</param>
		/// <returns>True if the process succeeded</returns>
		static bool TerminateProcessesGenericPlatform(Predicate<FileReference> predicate, ILogger logger, CancellationToken cancellationToken)
		{
			bool result = true;
			Dictionary<(int, DateTime), int> processToCount = new Dictionary<(int, DateTime), int>();
			for (; ; )
			{
				cancellationToken.ThrowIfCancellationRequested();

				bool bNoMatches = true;

				// Enumerate all the processes
				Process[] processes = Process.GetProcesses();
				foreach (Process process in processes)
				{
					// Attempt to get the image file. Ignore exceptions trying to fetch metadata for processes we don't have access to.
					FileReference? imageFile;
					try
					{
						imageFile = new FileReference(process.MainModule.FileName);
					}
					catch
					{
						imageFile = null;
					}

					// Test whether to terminate this process
					if (imageFile != null && predicate(imageFile))
					{
						// Get a unique id for the process, given that process ids are recycled
						(int, DateTime) uniqueId;
						try
						{
							uniqueId = (process.Id, process.StartTime);
						}
						catch
						{
							uniqueId = (process.Id, DateTime.MinValue);
						}

						// Figure out whether to try and terminate this process
						const int MaxCount = 5;
						if (!processToCount.TryGetValue(uniqueId, out int count) || count < MaxCount)
						{
							bNoMatches = false;
							try
							{
								logger.LogInformation("Terminating {ImageName} ({ProcessId})", imageFile, process.Id);
								process.Kill(true);
								if (!process.WaitForExit(5 * 1000))
								{
									logger.LogInformation("Termination still pending; will retry...");
								}
							}
							catch (Exception ex)
							{
								count++;
								if (count > 1)
								{
									logger.LogInformation(ex, "Exception while querying basic process info for pid {ProcessId}; will retry.", process.Id);
								}
								else if (count == MaxCount)
								{
									logger.LogWarning(ex, "Unable to terminate process {ImageFile} ({ProcessId}): {Message}", imageFile, process.Id, ex.Message);
									result = false;
								}
								processToCount[uniqueId] = count;
							}
						}
					}
				}

				// Return once we reach this point and haven't found anything else to terminate
				if (bNoMatches)
				{
					return result;
				}
			}
		}
	}
}
