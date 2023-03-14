// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Management;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;
using System.ServiceProcess;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Commands
{
	/// <summary>
	/// Upgrades a running service to the current application
	/// </summary>
	[Command("Service", "Upgrade", "Replaces a running service with the application in the current directory")]
	class UpgradeCommand : Command
	{
		/// <summary>
		/// The process id to replace
		/// </summary>
		[CommandLine("-ProcessId=", Required = true)]
		int ProcessId { get; set; } = -1;

		/// <summary>
		/// The target directory to install to
		/// </summary>
		[CommandLine("-TargetDir=", Required = true)]
		DirectoryReference TargetDir { get; set; } = null!;

		/// <summary>
		/// Arguments to forwar to the target executable
		/// </summary>
		[CommandLine("-Arguments=", Required = true)]
		string Arguments { get; set; } = null!;

		/// <summary>
		/// Upgrades the application to a new version
		/// </summary>
		/// <param name="logger">The log output device</param>
		/// <returns>True if the upgrade succeeded</returns>
		public override Task<int> ExecuteAsync(ILogger logger)
		{
			// Stop the other process
			logger.LogInformation("Attempting to perform upgrade on process {ProcessId}", ProcessId);
			using (Process otherProcess = Process.GetProcessById(ProcessId))
			{
				// Get the directory containing the target application
				DirectoryInfo targetDir = new DirectoryInfo(TargetDir.FullName);
				HashSet<string> targetFiles = new HashSet<string>(targetDir.EnumerateFiles("*", SearchOption.AllDirectories).Select(x => x.FullName), StringComparer.OrdinalIgnoreCase);

				// Find all the source files
				DirectoryInfo sourceDir = new DirectoryInfo(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location)!);
				HashSet<string> sourceFiles = new HashSet<string>(sourceDir.EnumerateFiles("*", SearchOption.AllDirectories).Select(x => x.FullName), StringComparer.OrdinalIgnoreCase);

				// Exclude all the source files from the list of target files, since we may be in a subdirectory
				targetFiles.ExceptWith(sourceFiles);

				// Ignore any files that are in the saved directory
				string sourceDataDir = Path.Combine(sourceDir.FullName, "Saved") + Path.DirectorySeparatorChar;
				sourceFiles.RemoveWhere(x => x.StartsWith(sourceDataDir, StringComparison.OrdinalIgnoreCase));

				string targetDataDir = Path.Combine(targetDir.FullName, "Saved") + Path.DirectorySeparatorChar;
				targetFiles.RemoveWhere(x => x.StartsWith(targetDataDir, StringComparison.OrdinalIgnoreCase));

				// Copy all the files into the target directory
				List<Tuple<string, string>> renameFiles = new List<Tuple<string, string>>();
				foreach (string sourceFile in sourceFiles)
				{
					if (!sourceFile.StartsWith(sourceDir.FullName, StringComparison.OrdinalIgnoreCase))
					{
						throw new InvalidDataException($"Expected {sourceFile} to be under {sourceDir.FullName}");
					}

					string targetFile = targetDir.FullName + sourceFile.Substring(sourceDir.FullName.Length);
					Directory.CreateDirectory(Path.GetDirectoryName(targetFile)!);

					string targetFileBeforeRename = targetFile + ".new";
					logger.LogDebug("Copying {SourceFile} to {TargetFileBeforeRename}", sourceFile, targetFileBeforeRename);
					File.Copy(sourceFile, targetFileBeforeRename, true);

					renameFiles.Add(Tuple.Create(targetFileBeforeRename, targetFile));
					targetFiles.Remove(targetFileBeforeRename);
				}

				if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
				{
					UpgradeWindowsService(logger, otherProcess, targetFiles, renameFiles);
				}
				else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
				{
					UpgradeMacService(logger, otherProcess, targetFiles, renameFiles);
				}
				else if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
				{
					UpgradeLinuxService(logger, otherProcess, targetFiles, renameFiles);
				}
				else
				{
					logger.LogError("Agent is not running a platform that supports Upgrades. Platform: {Platform}", RuntimeInformation.OSDescription);
					return Task.FromResult(-1);
				}
			}
			logger.LogInformation("Upgrade complete");
			return Task.FromResult(0);
		}

		static void UpgradeFilesInPlace(ILogger logger, HashSet<string> targetFiles, List<Tuple<string, string>> renameFiles)
		{
			// Remove all the target files
			foreach (string targetFile in targetFiles)
			{
				logger.LogDebug("Deleting {File}", targetFile);
				File.SetAttributes(targetFile, FileAttributes.Normal);
				File.Delete(targetFile);
			}

			// Rename all the new files into place
			foreach (Tuple<string, string> pair in renameFiles)
			{
				logger.LogDebug("Renaming {SourceFile} to {TargetFile}", pair.Item1, pair.Item2);
				File.Move(pair.Item1, pair.Item2, true);
			}
		}

		static void UpgradeMacService(ILogger logger, Process otherProcess, HashSet<string> targetFiles, List<Tuple<string, string>> renameFiles)
		{
			UpgradeFilesInPlace(logger, targetFiles, renameFiles);
			logger.LogDebug("Upgrade completed, restarting...");
			otherProcess.Kill();
		}

		static void UpgradeLinuxService(ILogger logger, Process otherProcess, HashSet<string> targetFiles, List<Tuple<string, string>> renameFiles)
		{
			UpgradeFilesInPlace(logger, targetFiles, renameFiles);
			logger.LogDebug("Upgrade completed, restarting...");
			otherProcess.Kill();
		}

		[SupportedOSPlatform("windows")]
		void UpgradeWindowsService(ILogger logger, Process otherProcess, HashSet<string> targetFiles, List<Tuple<string, string>> renameFiles)
		{
			// Try to get the service associated with the passed-in process id
			using (ServiceController? service = GetServiceForProcess(ProcessId))
			{
				// Stop the process
				if (service == null)
				{
					logger.LogInformation("Terminating other process");
					otherProcess.Kill();
				}
				else
				{
					logger.LogInformation("Stopping service");
					service.Stop();
				}
				otherProcess.WaitForExit();

				UpgradeFilesInPlace(logger, targetFiles, renameFiles);

				// Run the new application
				if (service == null)
				{
					string driverFileName = "dotnet";
					string assemblyFileName = Path.Combine(TargetDir.FullName, Path.GetFileName(Assembly.GetExecutingAssembly().Location));

					StringBuilder driverArguments = new StringBuilder();
					driverArguments.AppendArgument(assemblyFileName);
					driverArguments.Append(' ');
					driverArguments.Append(Arguments);

					StringBuilder launch = new StringBuilder();
					launch.AppendArgument(driverFileName);
					launch.Append(' ');
					launch.Append(driverArguments);
					logger.LogInformation("Launching: {Launch}", launch.ToString());

					using Process newProcess = Process.Start(driverFileName, driverArguments.ToString());
				}
				else
				{
					// Start the service again
					logger.LogInformation("Restarting service");
					service.Start();
				}
			}
		}

		/// <summary>
		/// Try to find the service controller for the given process id
		/// </summary>
		/// <param name="processId">The process id to search for</param>
		/// <returns>The service controller corresponding to this process</returns>
		[SupportedOSPlatform("windows")]
		static ServiceController? GetServiceForProcess(int processId)
		{
			try
			{
				using (ManagementObjectSearcher searcher = new ManagementObjectSearcher($"SELECT Name FROM Win32_Service WHERE ProcessId={processId}"))
				{
					foreach (ManagementObject service in searcher.Get())
					{
						PropertyData property = service.Properties["Name"];
						if (property != null)
						{
							string? name = property.Value as string;
							if (name != null)
							{
								return new ServiceController(name);
							}
						}
					}
				}
			}
			catch
			{
			}
			return null;
		}
	}
}
