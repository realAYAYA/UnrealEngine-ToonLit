// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;
using System.ServiceProcess;
using System.Text;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using Microsoft.Management.Infrastructure;

namespace Horde.Agent.Commands.Service
{
	/// <summary>
	/// Upgrades a running service to the current application
	/// </summary>
	[Command("service", "upgrade", "Replaces a running service with the application in the current directory", Advertise = false)]
	class UpgradeCommand : Command
	{
		/// <summary>
		/// The process ID to replace (the old but currently running agent process)
		/// </summary>
		[CommandLine("-ProcessId=", Required = true)]
		int ProcessId { get; set; } = -1;

		/// <summary>
		/// The target directory to install to
		/// </summary>
		[CommandLine("-TargetDir=", Required = true)]
		DirectoryReference TargetDir { get; set; } = null!;

		/// <summary>
		/// Arguments to forward to the target executable
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
				DirectoryInfo sourceDir = new(AppContext.BaseDirectory);
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

					string targetFile = Path.Combine(targetDir.FullName, sourceFile.Substring(sourceDir.FullName.Length));
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
					logger.LogError("Agent is not running a platform that supports upgrades. Platform: {Platform}", RuntimeInformation.OSDescription);
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
			// Assume agent process is auto-restarted by OS or external daemon process handler (such as launchd)
		}

		static void UpgradeLinuxService(ILogger logger, Process otherProcess, HashSet<string> targetFiles, List<Tuple<string, string>> renameFiles)
		{
			UpgradeFilesInPlace(logger, targetFiles, renameFiles);
			logger.LogDebug("Upgrade completed, restarting...");
			otherProcess.Kill();
			// Assume agent process is auto-restarted by OS or external daemon process handler (such as systemd)
		}

		[SupportedOSPlatform("windows")]
		void UpgradeWindowsService(ILogger logger, Process otherProcess, HashSet<string> targetFiles, List<Tuple<string, string>> renameFiles)
		{
			// Try to get the service associated with the passed-in process id
			using ServiceController? service = GetServiceForProcess(ProcessId);

			// Stop the process
			if (service == null)
			{
				logger.LogInformation("Terminating running agent process...");
				otherProcess.Kill();
			}
			else
			{
				logger.LogInformation("Stopping service...");
				service.Stop();
			}
			otherProcess.WaitForExit();

			UpgradeFilesInPlace(logger, targetFiles, renameFiles);

			// Run the new application
			if (service == null)
			{
				string executable;
				StringBuilder arguments = new();
				if (AgentApp.IsSelfContained)
				{
					if (Environment.ProcessPath == null)
					{
						throw new Exception("Unable to detect current process path");
					}

					executable = Path.Combine(TargetDir.FullName, Path.GetFileName(Environment.ProcessPath));
					if (!File.Exists(executable))
					{
						throw new Exception($"{executable} not found. Is the new agent software packaged as self-contained?");
					}
				}
				else
				{
#pragma warning disable IL3000 // Avoid accessing Assembly file path when publishing as a single file
					executable = "dotnet";
					string assemblyFileName = Path.Combine(TargetDir.FullName, Path.GetFileName(Assembly.GetExecutingAssembly().Location));
					arguments.AppendArgument(assemblyFileName);
#pragma warning restore IL3000 // Avoid accessing Assembly file path when publishing as a single file					
				}
				arguments.Append(' ');
				arguments.Append(Arguments);

				logger.LogInformation("Launching: {Executable} {Arguments}", executable, arguments.ToString());
				using Process newProcess = Process.Start(executable, arguments.ToString());
			}
			else
			{
				// Start the service again
				logger.LogInformation("Restarting service...");
				service.Start();
			}
		}

		/// <summary>
		/// Try to find the service controller for the given process id
		/// </summary>
		/// <param name="processId">The process id to search for</param>
		/// <returns>The service controller corresponding to this process</returns>
		[SupportedOSPlatform("windows")]
		public static ServiceController? GetServiceForProcess(int processId)
		{
			try
			{
				using (CimSession session = CimSession.Create(null))
				{
					const string QueryNamespace = @"root\cimv2";
					const string QueryDialect = "WQL";

					foreach (CimInstance instance in session.QueryInstances(QueryNamespace, QueryDialect, $"SELECT Name FROM Win32_Service WHERE ProcessId={processId}"))
					{
						foreach (CimProperty property in instance.CimInstanceProperties)
						{
							if (property.Name.Equals("Name", StringComparison.OrdinalIgnoreCase))
							{
								string? value = property.Value as string;
								if (value != null)
								{
									return new ServiceController(value);
								}
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
