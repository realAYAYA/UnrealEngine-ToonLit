// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Net.Http;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Grpc.Core;
using Horde.Agent.Parser;
using Horde.Agent.Services;
using Horde.Agent.Utility;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Leases.Handlers
{
	class UpgradeHandler : LeaseHandler<UpgradeTask>
	{
		readonly IServerLoggerFactory _serverLoggerFactory;

		public UpgradeHandler(IServerLoggerFactory serverLoggerFactory)
		{
			_serverLoggerFactory = serverLoggerFactory;
		}

		/// <inheritdoc/>
		public override async Task<LeaseResult> ExecuteAsync(ISession session, string leaseId, UpgradeTask task, CancellationToken cancellationToken)
		{
			await using IServerLogger logger = _serverLoggerFactory.CreateLogger(session, task.LogId);

			string requiredVersion = task.SoftwareId;

			// Check if we're running the right version
			if (requiredVersion != null && requiredVersion != Program.Version)
			{
				logger.LogInformation("Upgrading from {CurrentVersion} to {TargetVersion}", Program.Version, requiredVersion);

				// Clear out the working directory
				DirectoryReference upgradeDir = DirectoryReference.Combine(session.WorkingDir, "Upgrade");
				DirectoryReference.CreateDirectory(upgradeDir);
				await DeleteDirectoryContentsAsync(new DirectoryInfo(upgradeDir.FullName));

				// Download the new software
				FileInfo outputFile = new FileInfo(Path.Combine(upgradeDir.FullName, "Agent.zip"));
				using (IRpcClientRef<HordeRpc.HordeRpcClient> rpcClientRef = await session.RpcConnection.GetClientRefAsync<HordeRpc.HordeRpcClient>(cancellationToken))
				using (AsyncServerStreamingCall<DownloadSoftwareResponse> cursor = rpcClientRef.Client.DownloadSoftware(new DownloadSoftwareRequest(requiredVersion), null, null, cancellationToken))
				{
					using (Stream outputStream = outputFile.Open(FileMode.Create))
					{
						while (await cursor.ResponseStream.MoveNext(cancellationToken))
						{
							outputStream.Write(cursor.ResponseStream.Current.Data.Span);
						}
					}
				}

				// Extract it to a temporary directory
				DirectoryReference extractedDir = DirectoryReference.Combine(upgradeDir, "Extracted");
				DirectoryReference.CreateDirectory(extractedDir);
				ZipFile.ExtractToDirectory(outputFile.FullName, extractedDir.FullName);

				//				// Debug code for updating an agent with the local version
				//				foreach (FileInfo SourceFile in new FileInfo(Assembly.GetExecutingAssembly().Location).Directory.EnumerateFiles())
				//				{
				//					SourceFile.CopyTo(Path.Combine(ExtractedDir.FullName, SourceFile.Name), true);
				//				}

				// Get the current process and assembly. This may be different if running through dotnet.exe rather than a native PE image.
				FileReference assemblyFileName = new FileReference(Assembly.GetExecutingAssembly().Location);

				// Spawn the other process
				using (Process process = new Process())
				{
					StringBuilder arguments = new StringBuilder();

					DirectoryReference targetDir = assemblyFileName.Directory;

					// We were launched via an external application (presumably dotnet.exe). Do the same thing again.
					FileReference newAssemblyFileName = FileReference.Combine(extractedDir, assemblyFileName.MakeRelativeTo(targetDir));
					if (!FileReference.Exists(newAssemblyFileName))
					{
						logger.LogError("Unable to find {AgentExe} in extracted archive", newAssemblyFileName);
						return LeaseResult.Failed;
					}

					process.StartInfo.FileName = "dotnet";

					StringBuilder currentArguments = new StringBuilder();
					foreach (string arg in Program.Args)
					{
						currentArguments.AppendArgument(arg);
					}

					arguments.AppendArgument(newAssemblyFileName.FullName);
					arguments.AppendArgument("Service");
					arguments.AppendArgument("Upgrade");
					arguments.AppendArgument("-ProcessId=", Environment.ProcessId.ToString());
					arguments.AppendArgument("-TargetDir=", targetDir.FullName);
					arguments.AppendArgument("-Arguments=", currentArguments.ToString());

					process.StartInfo.Arguments = arguments.ToString();
					process.StartInfo.UseShellExecute = false;
					process.EnableRaisingEvents = true;

					StringBuilder launchCommand = new StringBuilder();
					launchCommand.AppendArgument(process.StartInfo.FileName);
					launchCommand.Append(' ');
					launchCommand.Append(arguments);
					logger.LogInformation("Launching: {Launch}", launchCommand.ToString());

					TaskCompletionSource<int> exitCodeSource = new TaskCompletionSource<int>();
					process.Exited += (sender, args) => { exitCodeSource.SetResult(process.ExitCode); };

					process.Start();

					using (cancellationToken.Register(() => { exitCodeSource.SetResult(0); }))
					{
						await exitCodeSource.Task;
					}
				}
			}

			return LeaseResult.Success;
		}

		/// <summary>
		/// Delete the contents of a directory without deleting it itself
		/// </summary>
		/// <param name="baseDir">Directory to clean</param>
		/// <returns>Async task</returns>
		static async Task DeleteDirectoryContentsAsync(DirectoryInfo baseDir)
		{
			List<Task> childTasks = new List<Task>();
			foreach (DirectoryInfo subDir in baseDir.EnumerateDirectories())
			{
				childTasks.Add(Task.Run(() => DeleteDirectoryAsync(subDir)));
			}
			foreach (FileInfo file in baseDir.EnumerateFiles())
			{
				file.Attributes = FileAttributes.Normal;
				file.Delete();
			}
			foreach (Task childTask in childTasks)
			{
				await childTask;
			}
		}

		/// <summary>
		/// Deletes a directory and its contents
		/// </summary>
		/// <param name="baseDir">Directory to delete</param>
		/// <returns>Async task</returns>
		static async Task DeleteDirectoryAsync(DirectoryInfo baseDir)
		{
			await DeleteDirectoryContentsAsync(baseDir);
			baseDir.Delete();
		}
	}
}

