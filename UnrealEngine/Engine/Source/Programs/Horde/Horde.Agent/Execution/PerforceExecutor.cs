// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Perforce;
using EpicGames.Perforce.Managed;
using Horde.Agent.Utility;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Messages;
using Microsoft.Extensions.Logging;
using OpenTracing;
using OpenTracing.Util;

namespace Horde.Agent.Execution
{
	class PerforceExecutor : BuildGraphExecutor
	{
		protected AgentWorkspace? _autoSdkWorkspaceInfo;
		protected AgentWorkspace _workspaceInfo;
		protected DirectoryReference _rootDir;
		protected DirectoryReference? _sharedStorageDir;

		protected WorkspaceInfo? _autoSdkWorkspace;
		protected WorkspaceInfo _workspace;

		protected Dictionary<string, string> _envVars = new Dictionary<string, string>();

		public PerforceExecutor(IRpcConnection rpcConnection, string jobId, string batchId, string agentTypeName, AgentWorkspace? autoSdkWorkspaceInfo, AgentWorkspace workspaceInfo, DirectoryReference rootDir)
			: base(rpcConnection, jobId, batchId, agentTypeName)
		{
			_autoSdkWorkspaceInfo = autoSdkWorkspaceInfo;
			_workspaceInfo = workspaceInfo;
			_rootDir = rootDir;

			_workspace = null!;
		}

		public override async Task InitializeAsync(ILogger logger, CancellationToken cancellationToken)
		{
			await base.InitializeAsync(logger, cancellationToken);

			// Setup and sync the autosdk workspace
			if (_autoSdkWorkspaceInfo != null)
			{
				using (IScope ccope = GlobalTracer.Instance.BuildSpan("Workspace").WithResourceName("AutoSDK").StartActive())
				{
					_autoSdkWorkspace = await Utility.WorkspaceInfo.SetupWorkspaceAsync(_autoSdkWorkspaceInfo, _rootDir, logger, cancellationToken);

					DirectoryReference legacyDir = DirectoryReference.Combine(_autoSdkWorkspace.MetadataDir, "HostWin64");
					if (DirectoryReference.Exists(legacyDir))
					{
						try
						{
							FileUtils.ForceDeleteDirectory(legacyDir);
						}
						catch(Exception ex)
						{
							logger.LogInformation(ex, "Unable to delete {Dir}", legacyDir);
						}
					}

					int autoSdkChangeNumber = await _autoSdkWorkspace.GetLatestChangeAsync(cancellationToken);

					string syncText = $"Synced to CL {autoSdkChangeNumber}";

					FileReference syncFile = FileReference.Combine(_autoSdkWorkspace.MetadataDir, "Synced.txt");
					if (!FileReference.Exists(syncFile) || FileReference.ReadAllText(syncFile) != syncText)
					{
						FileReference.Delete(syncFile);

						FileReference autoSdkCacheFile = FileReference.Combine(_autoSdkWorkspace.MetadataDir, "Contents.dat");
						await WorkspaceInfo.UpdateLocalCacheMarker(autoSdkCacheFile, autoSdkChangeNumber, -1);
						await _autoSdkWorkspace.SyncAsync(autoSdkChangeNumber, -1, autoSdkCacheFile, cancellationToken);

						await FileReference.WriteAllTextAsync(syncFile, syncText);
					}
				}
			}

			using (IScope scope = GlobalTracer.Instance.BuildSpan("Workspace").WithResourceName(_workspaceInfo.Identifier).StartActive())
			{
				// Sync the regular workspace
				_workspace = await Utility.WorkspaceInfo.SetupWorkspaceAsync(_workspaceInfo, _rootDir, logger, cancellationToken);

				// Figure out the change to build
				if (_job.Change == 0)
				{
					List<ChangesRecord> changes = await _workspace.PerforceClient.GetChangesAsync(ChangesOptions.None, 1, ChangeStatus.Submitted, new[] { _stream.Name + "/..." }, cancellationToken);
					_job.Change = changes[0].Number;

					UpdateJobRequest updateJobRequest = new UpdateJobRequest();
					updateJobRequest.JobId = _jobId;
					updateJobRequest.Change = _job.Change;
					await _rpcConnection.InvokeAsync(x => x.UpdateJobAsync(updateJobRequest, null, null, cancellationToken), new RpcContext(), cancellationToken);
				}

				// Sync the workspace
				int syncPreflightChange = (_job.ClonedPreflightChange != 0) ? _job.ClonedPreflightChange : _job.PreflightChange;
				await _workspace.SyncAsync(_job.Change, syncPreflightChange, null, cancellationToken);

				// Remove any cached BuildGraph manifests
				DirectoryReference manifestDir = DirectoryReference.Combine(_workspace.WorkspaceDir, "Engine", "Saved", "BuildGraph");
				if (DirectoryReference.Exists(manifestDir))
				{
					try
					{
						FileUtils.ForceDeleteDirectoryContents(manifestDir);
					}
					catch (Exception ex)
					{
						logger.LogWarning(ex, "Unable to delete contents of {ManifestDir}", manifestDir);
					}
				}
			}

			// Remove all the local settings directories
			DeleteEngineUserSettings(logger);

			// Get the temp storage directory
			if (!String.IsNullOrEmpty(_agentType!.TempStorageDir))
			{
				string escapedStreamName = Regex.Replace(_stream!.Name, "[^a-zA-Z0-9_-]", "+");
				_sharedStorageDir = DirectoryReference.Combine(new DirectoryReference(_agentType!.TempStorageDir), escapedStreamName, $"CL {_job!.Change} - Job {_jobId}");
				CopyAutomationTool(_sharedStorageDir, _workspace.WorkspaceDir, logger);
			}

			// Get all the environment variables for jobs
			_envVars["IsBuildMachine"] = "1";
			_envVars["uebp_LOCAL_ROOT"] = _workspace.WorkspaceDir.FullName;
			_envVars["uebp_PORT"] = _workspace.ServerAndPort;
			_envVars["uebp_USER"] = _workspace.UserName;
			_envVars["uebp_CLIENT"] = _workspace.ClientName;
			_envVars["uebp_BuildRoot_P4"] = _stream!.Name;
			_envVars["uebp_BuildRoot_Escaped"] = _stream!.Name.Replace('/', '+');
			_envVars["uebp_CLIENT_ROOT"] = $"//{_workspace.ClientName}";
			_envVars["uebp_CL"] = _job!.Change.ToString();
			_envVars["uebp_CodeCL"] = _job!.CodeChange.ToString();
			_envVars["P4USER"] = _workspace.UserName;
			_envVars["P4CLIENT"] = _workspace.ClientName;

			if (_autoSdkWorkspace != null)
			{
				_envVars["UE_SDKS_ROOT"] = _autoSdkWorkspace.WorkspaceDir.FullName;
			}
		}

		private static void DeleteEngineUserSettings(ILogger logger)
		{
			if(RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				DirectoryReference? appDataDir = DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.LocalApplicationData);
				if(appDataDir != null)
				{
					string[] dirNames = { "Unreal Engine", "UnrealEngine", "UnrealEngineLauncher", "UnrealHeaderTool", "UnrealPak" };
					DeleteEngineUserSettings(appDataDir, dirNames, logger);
				}
			}
			else if(RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
			{
				string? homeDir = Environment.GetEnvironmentVariable("HOME");
				if(!String.IsNullOrEmpty(homeDir))
				{
					string[] dirNames = { "Library/Preferences/Unreal Engine", "Library/Application Support/Epic" };
					DeleteEngineUserSettings(new DirectoryReference(homeDir), dirNames, logger);
				}
			}
		}

		private static void DeleteEngineUserSettings(DirectoryReference baseDir, string[] subDirNames, ILogger logger)
		{
			foreach (string subDirName in subDirNames)
			{
				DirectoryReference settingsDir = DirectoryReference.Combine(baseDir, subDirName);
				if (DirectoryReference.Exists(settingsDir))
				{
					logger.LogInformation("Removing local settings directory ({SettingsDir})...", settingsDir);
					try
					{
						FileUtils.ForceDeleteDirectory(settingsDir);
					}
					catch (Exception ex)
					{
						logger.LogWarning(ex, "Error while deleting directory.");
					}
				}
			}
		}

		PerforceLogger CreatePerforceLogger(ILogger logger)
		{
			PerforceLogger perforceLogger = new PerforceLogger(logger);
			perforceLogger.AddClientView(_workspace.WorkspaceDir, _workspace.StreamView, _job.Change);
			if (_autoSdkWorkspace != null)
			{
				perforceLogger.AddClientView(_autoSdkWorkspace.WorkspaceDir, _autoSdkWorkspace.StreamView, _job.Change);
			}
			return perforceLogger;
		}

		protected override async Task<bool> SetupAsync(BeginStepResponse step, ILogger logger, CancellationToken cancellationToken)
		{
			PerforceLogger perforceLogger = CreatePerforceLogger(logger);
			return await SetupAsync(step, _workspace.WorkspaceDir, _sharedStorageDir, _envVars, perforceLogger, cancellationToken);
		}

		protected override async Task<bool> ExecuteAsync(BeginStepResponse step, ILogger logger, CancellationToken cancellationToken)
		{
			PerforceLogger perforceLogger = CreatePerforceLogger(logger);
			return await ExecuteAsync(step, _workspace.WorkspaceDir, _sharedStorageDir, _envVars, perforceLogger, cancellationToken);
		}

		public override async Task FinalizeAsync(ILogger logger, CancellationToken cancellationToken)
		{
			await _workspace.CleanAsync(cancellationToken);
		}

		public static async Task ConformAsync(DirectoryReference rootDir, IList<AgentWorkspace> pendingWorkspaces, bool removeUntrackedFiles, ILogger logger, CancellationToken cancellationToken)
		{
			// Print out all the workspaces we're going to sync
			logger.LogInformation("Workspaces:");
			foreach (AgentWorkspace pendingWorkspace in pendingWorkspaces)
			{
				logger.LogInformation("  Identifier={Identifier}, Stream={StreamName}, Incremental={Incremental}", pendingWorkspace.Identifier, pendingWorkspace.Stream, pendingWorkspace.Incremental);
			}

			// Make workspaces for all the unique configurations on this agent
			List<WorkspaceInfo> workspaces = new List<WorkspaceInfo>();
			foreach (AgentWorkspace pendingWorkspace in pendingWorkspaces)
			{
				WorkspaceInfo workspace = await Utility.WorkspaceInfo.SetupWorkspaceAsync(pendingWorkspace, rootDir, logger, cancellationToken);
				workspaces.Add(workspace);
			}

			// Find all the unique Perforce servers
			List<IPerforceConnection> perforceConnections = new List<IPerforceConnection>();
			try
			{
				foreach (WorkspaceInfo workspace in workspaces)
				{
					if (!perforceConnections.Any(x => x.Settings.ServerAndPort!.Equals(workspace.ServerAndPort, StringComparison.OrdinalIgnoreCase) && x.Settings.UserName!.Equals(workspace.PerforceClient.Settings.UserName, StringComparison.Ordinal)))
					{
						IPerforceConnection connection = await PerforceConnection.CreateAsync(workspace.PerforceClient.Settings, workspace.PerforceClient.Logger);
						perforceConnections.Add(connection);
					}
				}

				// Enumerate all the workspaces
				foreach (IPerforceConnection perforce in perforceConnections)
				{
					// Get the server info
					InfoRecord info = await perforce.GetInfoAsync(InfoOptions.ShortOutput, CancellationToken.None);

					// Enumerate all the clients on the server
					List<ClientsRecord> clients = await perforce.GetClientsAsync(ClientsOptions.None, null, -1, null, perforce.Settings.UserName, cancellationToken);
					foreach (ClientsRecord client in clients)
					{
						// Check the host matches
						if (!String.Equals(client.Host, info.ClientHost, StringComparison.OrdinalIgnoreCase))
						{
							continue;
						}

						// Check the edge server id matches
						if (!String.IsNullOrEmpty(client.ServerId) && !String.Equals(client.ServerId, info.ServerId, StringComparison.OrdinalIgnoreCase))
						{
							continue;
						}

						// Check it's under the managed root directory
						DirectoryReference? clientRoot;
						try
						{
							clientRoot = new DirectoryReference(client.Root);
						}
						catch
						{
							clientRoot = null;
						}

						if (clientRoot == null || !clientRoot.IsUnderDirectory(rootDir))
						{
							continue;
						}

						// Check it doesn't match one of the workspaces we want to keep
						if (workspaces.Any(x => String.Equals(client.Name, x.ClientName, StringComparison.OrdinalIgnoreCase)))
						{
							continue;
						}

						// Revert all the files in this clientspec and delete it
						logger.LogInformation("Deleting client {ClientName}...", client.Name);
						using IPerforceConnection perforceClient = await perforce.WithClientAsync(client.Name);
						await WorkspaceInfo.RevertAllChangesAsync(perforceClient, logger, cancellationToken);
						await perforce.DeleteClientAsync(DeleteClientOptions.None, client.Name, cancellationToken);
					}
				}

				// Delete all the directories that aren't a workspace root
				if (DirectoryReference.Exists(rootDir))
				{
					// Delete all the files in the root
					foreach (FileInfo file in rootDir.ToDirectoryInfo().EnumerateFiles())
					{
						FileUtils.ForceDeleteFile(file);
					}

					// Build a set of directories to protect
					HashSet<DirectoryReference> protectDirs = new HashSet<DirectoryReference>();
					if (!removeUntrackedFiles)
					{
						protectDirs.Add(DirectoryReference.Combine(rootDir, "Temp"));
						protectDirs.Add(DirectoryReference.Combine(rootDir, "Saved"));
					}
					protectDirs.UnionWith(workspaces.Select(x => x.MetadataDir));

					// Delete all the directories which aren't a workspace root
					foreach (DirectoryReference dir in DirectoryReference.EnumerateDirectories(rootDir))
					{
						if (protectDirs.Contains(dir))
						{
							logger.LogInformation("Keeping directory {KeepDir}", dir);
						}
						else
						{
							logger.LogInformation("Deleting directory {DeleteDir}", dir);
							FileUtils.ForceDeleteDirectory(dir);
						}
					}
					logger.LogInformation("");
				}

				// Revert any open files in any workspace
				HashSet<string> revertedClientNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
				foreach (WorkspaceInfo workspace in workspaces)
				{
					if (revertedClientNames.Add(workspace.ClientName))
					{
						using IPerforceConnection connection = await workspace.PerforceClient.WithClientAsync(workspace.ClientName);
						await workspace.Repository.RevertAsync(connection, cancellationToken);
					}
				}

				// Sync all the branches.
				List<Func<Task>> syncFuncs = new List<Func<Task>>();
				foreach (IGrouping<DirectoryReference, WorkspaceInfo> workspaceGroup in workspaces.GroupBy(x => x.MetadataDir).OrderBy(x => x.Key.FullName))
				{
					List<PopulateRequest> populateRequests = new List<PopulateRequest>();
					foreach (WorkspaceInfo workspace in workspaceGroup)
					{
						IPerforceConnection perforceClient = await workspace.PerforceClient.WithClientAsync(workspace.ClientName);
						perforceConnections.Add(perforceClient);
						populateRequests.Add(new PopulateRequest(perforceClient, workspace.StreamName, workspace.View));
					}

					WorkspaceInfo? firstWorkspace = workspaceGroup.First();
					if (populateRequests.Count == 1 && !firstWorkspace.RemoveUntrackedFiles && !removeUntrackedFiles)
					{
						await firstWorkspace.CleanAsync(cancellationToken);
						syncFuncs.Add(() => firstWorkspace.SyncAsync(-1, -1, null, cancellationToken));
					}
					else
					{
						ManagedWorkspace repository = firstWorkspace.Repository;
						Tuple<int, StreamSnapshot>[] streamStates = await repository.PopulateCleanAsync(populateRequests, cancellationToken);
						syncFuncs.Add(() => repository.PopulateSyncAsync(populateRequests, streamStates, false, cancellationToken));
					}
				}
				foreach (Func<Task> syncFunc in syncFuncs)
				{
					await syncFunc();
				}
			}
			finally
			{
				foreach (IPerforceConnection perforceConnection in perforceConnections)
				{
					perforceConnection.Dispose();
				}
			}
		}
	}
}
