// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Perforce;
using EpicGames.Perforce.Managed;
using HordeCommon.Rpc.Messages;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Utility
{
	/// <summary>
	/// Stores information about a managed Perforce workspace
	/// </summary>
	class WorkspaceInfo
	{
		/// <summary>
		/// The perforce connection
		/// </summary>
		public IPerforceConnection PerforceClient
		{
			get;
		}

		/// <summary>
		/// The Perforce server and port. This is checked to not be null in the constructor.
		/// </summary>
		public string ServerAndPort => PerforceClient.Settings.ServerAndPort!;

		/// <summary>
		/// The Perforce client name. This is checked to not be null in the constructor.
		/// </summary>
		public string ClientName => PerforceClient.Settings.ClientName!;

		/// <summary>
		/// The Perforce user name. This is checked to not be null in the constructor.
		/// </summary>
		public string UserName => PerforceClient.Settings.UserName!;

		/// <summary>
		/// The hostname
		/// </summary>
		public string HostName { get; }

		/// <summary>
		/// The current stream
		/// </summary>
		public string StreamName { get; }

		/// <summary>
		/// View for the stream
		/// </summary>
		public PerforceViewMap StreamView { get; }

		/// <summary>
		/// The directory containing metadata for this workspace
		/// </summary>
		public DirectoryReference MetadataDir { get; }

		/// <summary>
		/// The directory containing workspace data
		/// </summary>
		public DirectoryReference WorkspaceDir { get; }

		/// <summary>
		/// The view for files to sync
		/// </summary>
		public IReadOnlyList<string> View { get; }

		/// <summary>
		/// Whether untracked files should be removed from this workspace
		/// </summary>
		public bool RemoveUntrackedFiles { get; }

		/// <summary>
		/// The managed repository instance
		/// </summary>
		public ManagedWorkspace Repository { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="perforce">The perforce connection</param>
		/// <param name="hostName">Name of this host</param>
		/// <param name="streamName">Name of the stream to sync</param>
		/// <param name="streamView">Stream view onto the depot</param>
		/// <param name="metadataDir">Path to the metadata directory</param>
		/// <param name="workspaceDir">Path to the workspace directory</param>
		/// <param name="view">View for files to be synced</param>
		/// <param name="removeUntrackedFiles">Whether to remove untracked files when syncing</param>
		/// <param name="repository">The repository instance</param>
		public WorkspaceInfo(IPerforceConnection perforce, string hostName, string streamName, PerforceViewMap streamView, DirectoryReference metadataDir, DirectoryReference workspaceDir, IList<string>? view, bool removeUntrackedFiles, ManagedWorkspace repository)
		{
			PerforceClient = perforce;

			if (perforce.Settings.ClientName == null)
			{
				throw new ArgumentException("PerforceConnection does not have valid client name");
			}

			HostName = hostName;
			StreamName = streamName;
			StreamView = streamView;
			MetadataDir = metadataDir;
			WorkspaceDir = workspaceDir;
			View = (view == null) ? new List<string>() : new List<string>(view);
			RemoveUntrackedFiles = removeUntrackedFiles;
			Repository = repository;
		}

		/// <summary>
		/// Creates a new managed workspace
		/// </summary>
		/// <param name="workspace">The workspace definition</param>
		/// <param name="rootDir">Root directory for storing the workspace</param>
		/// <param name="logger">Logger output</param>
		/// <param name="cancellationToken">Cancellation token</param>
		/// <returns>New workspace info</returns>
		public static async Task<WorkspaceInfo> SetupWorkspaceAsync(AgentWorkspace workspace, DirectoryReference rootDir, ILogger logger, CancellationToken cancellationToken)
		{
			// Fill in the default credentials iff they are not set
			string? serverAndPort = String.IsNullOrEmpty(workspace.ServerAndPort)? null : workspace.ServerAndPort;
			string? userName = String.IsNullOrEmpty(workspace.UserName)? null : workspace.UserName;
			string? password = String.IsNullOrEmpty(workspace.Password)? null : workspace.Password;

			if (serverAndPort == null)
			{
				serverAndPort = PerforceSettings.Default.ServerAndPort;
				logger.LogInformation("Using locally configured Perforce server: '{ServerAndPort}'", serverAndPort);
			}

			if (userName == null)
			{
				userName = PerforceSettings.Default.UserName;
				logger.LogInformation("Using locally configured and logged in Perforce user: '{UserName}'", userName);
			}

			// Create the connection
			IPerforceConnection perforce = await PerforceConnection.CreateAsync(new PerforceSettings(serverAndPort, userName), logger);
			if (userName != null)
			{
				if (password != null)
				{
					await perforce.LoginAsync(password, cancellationToken);
				}
				else
				{
					logger.LogInformation("Using locally logged in session for {UserName}", userName);
				}
			}
			return await SetupWorkspaceAsync(perforce, workspace.Stream, workspace.Identifier, workspace.View, !workspace.Incremental, rootDir, logger, cancellationToken);
		}

		/// <summary>
		/// Creates a new managed workspace
		/// </summary>
		/// <param name="perforce">The perforce connection to use</param>
		/// <param name="streamName">The stream being synced</param>
		/// <param name="identifier">Identifier to use to share </param>
		/// <param name="view">View for this workspace</param>
		/// <param name="removeUntrackedFiles">Whether untracked files should be removed when cleaning this workspace</param>
		/// <param name="rootDir">Root directory for storing the workspace</param>
		/// <param name="logger">Logger output</param>
		/// <param name="cancellationToken">Cancellation token</param>
		/// <returns>New workspace info</returns>
		public static async Task<WorkspaceInfo> SetupWorkspaceAsync(IPerforceConnection perforce, string streamName, string identifier, IList<string> view, bool removeUntrackedFiles, DirectoryReference rootDir, ILogger logger, CancellationToken cancellationToken)
		{
			// Get the host name, and fill in any missing metadata about the connection
			InfoRecord info = await perforce.GetInfoAsync(InfoOptions.ShortOutput, cancellationToken);

			string? hostName = info.ClientHost;
			if(hostName == null)
			{
				throw new Exception("Unable to determine Perforce host name");
			}

			// replace invalid characters in the workspace identifier with a '+' character

			// append the slot index, if it's non-zero
			//			my $slot_idx = $optional_arguments->{'slot_idx'} || 0;
			//			$workspace_identifier .= sprintf("+%02d", $slot_idx) if $slot_idx;

			// if running on an edge server, append the server id to the client name
			string edgeSuffix = String.Empty;
			if(info.Services != null && info.ServerId != null)
			{
				string[] services = info.Services.Split(' ', StringSplitOptions.RemoveEmptyEntries);
				if (services.Any(x => x.Equals("edge-server", StringComparison.OrdinalIgnoreCase)))
				{
					edgeSuffix = $"+{info.ServerId}";
				}
			}

			// get all the workspace settings
			string clientName = $"Horde+{GetNormalizedHostName(hostName)}+{identifier}{edgeSuffix}";

			// Create the client Perforce connection
			IPerforceConnection perforceClient = await perforce.WithClientAsync(clientName);

			// Get the view for this stream
			StreamRecord stream = await perforceClient.GetStreamAsync(streamName, true, cancellationToken);
			PerforceViewMap streamView = PerforceViewMap.Parse(stream.View);

			// get the workspace names
			DirectoryReference metadataDir = DirectoryReference.Combine(rootDir, identifier);
			DirectoryReference workspaceDir = DirectoryReference.Combine(metadataDir, "Sync");

			// Create the repository
			ManagedWorkspace newRepository = await ManagedWorkspace.LoadOrCreateAsync(hostName, metadataDir, true, logger, cancellationToken);
			if (removeUntrackedFiles)
			{
				await newRepository.DeleteClientAsync(perforceClient, cancellationToken);
			}
			await newRepository.SetupAsync(perforceClient, streamName, cancellationToken);

			// Revert any open files
			await newRepository.RevertAsync(perforceClient, cancellationToken);

			// Create the workspace info
			return new WorkspaceInfo(perforceClient, hostName, streamName, streamView, metadataDir, workspaceDir, view, removeUntrackedFiles, newRepository);
		}

		/// <summary>
		/// Gets the latest change in the stream
		/// </summary>
		/// <param name="cancellationToken">Cancellation token</param>
		/// <returns>Latest changelist number</returns>
		public Task<int> GetLatestChangeAsync(CancellationToken cancellationToken)
		{
			return Repository.GetLatestChangeAsync(PerforceClient, StreamName, cancellationToken);
		}

		/// <summary>
		/// Revert any open files in the workspace and clean it
		/// </summary>
		/// <param name="cancellationToken">Cancellation token</param>
		/// <returns>Async task</returns>
		public async Task CleanAsync(CancellationToken cancellationToken)
		{
			await Repository.RevertAsync(PerforceClient, cancellationToken);
			await Repository.CleanAsync(RemoveUntrackedFiles, cancellationToken);
		}

		/// <summary>
		/// Removes the given cache file if it's invalid, and updates the metadata to reflect the version about to be synced
		/// </summary>
		/// <param name="cacheFile">Path to the cache file</param>
		/// <param name="change">The current change being built</param>
		/// <param name="preflightChange">The preflight changelist number</param>
		/// <returns>Async task</returns>
		public static async Task UpdateLocalCacheMarker(FileReference cacheFile, int change, int preflightChange)
		{
			// Create the new cache file descriptor
			string newDescriptor;
			if(preflightChange <= 0)
			{
				newDescriptor = $"CL {change}";
			}
			else
			{
				newDescriptor = $"CL {preflightChange} with base CL {change}";
			}

			// Remove the cache file if the current descriptor doesn't match
			FileReference descriptorFile = cacheFile.ChangeExtension(".txt");
			if (FileReference.Exists(cacheFile))
			{
				if (FileReference.Exists(descriptorFile))
				{
					string oldDescriptor = await FileReference.ReadAllTextAsync(descriptorFile);
					if (oldDescriptor == newDescriptor)
					{
						return;
					}
					else
					{
						FileReference.Delete(descriptorFile);
					}
				}
				FileReference.Delete(cacheFile);
			}

			// Write the new descriptor file
			await FileReference.WriteAllTextAsync(descriptorFile, newDescriptor);
		}

		/// <summary>
		/// Sync the workspace to a given changelist, and capture it so we can quickly clean in the future
		/// </summary>
		/// <param name="change">The changelist to sync to</param>
		/// <param name="preflightChange">Change to preflight, or 0</param>
		/// <param name="cacheFile">Path to the cache file to use</param>
		/// <param name="cancellationToken">Cancellation token</param>
		/// <returns>Async task</returns>
		public async Task SyncAsync(int change, int preflightChange, FileReference? cacheFile, CancellationToken cancellationToken)
		{
			await Repository.SyncAsync(PerforceClient, StreamName, change, View, RemoveUntrackedFiles, false, cacheFile, cancellationToken);

			// Purge the cache for incremental workspaces
			if (!RemoveUntrackedFiles)
			{
				await Repository.PurgeAsync(0, cancellationToken);
			}

			// if we're running a preflight
			if (preflightChange > 0)
			{
				await Repository.UnshelveAsync(PerforceClient, preflightChange, cancellationToken);
			}
		}

		/// <summary>
		/// Strip the domain suffix to get the host name
		/// </summary>
		/// <param name="hostName">Hostname with optional domain</param>
		/// <returns>Normalized host name</returns>
		static string GetNormalizedHostName(string hostName)
		{
			return Regex.Replace(hostName, @"\..*$", "").ToUpperInvariant();
		}

		/// <summary>
		/// Revert all files in a workspace
		/// </summary>
		public static async Task RevertAllChangesAsync(IPerforceConnection perforce, ILogger logger, CancellationToken cancellationToken)
		{
			// Make sure the client name is set
			if(perforce.Settings.ClientName == null)
			{
				throw new ArgumentException("RevertAllChangesAsync() requires PerforceConnection with client");
			}

			// check if there are any open files, and revert them
			List<OpenedRecord> files = await perforce.OpenedAsync(OpenedOptions.None, -1, null, null, 1, new[] { "//..." }, cancellationToken).ToListAsync(cancellationToken);
			if (files.Count > 0)
			{
				await perforce.RevertAsync(-1, null, RevertOptions.KeepWorkspaceFiles, new[] { "//..." }, cancellationToken);
			}

			// enumerate all the pending changelists
			List<ChangesRecord> pendingChanges = await perforce.GetChangesAsync(ChangesOptions.None, perforce.Settings.ClientName, -1, ChangeStatus.Pending, null, FileSpecList.Empty, cancellationToken);
			foreach (ChangesRecord pendingChange in pendingChanges)
			{
				// delete any shelved files if there are any
				List<DescribeRecord> records = await perforce.DescribeAsync(DescribeOptions.Shelved, -1, new int[] { pendingChange.Number }, cancellationToken);
				if (records.Count > 0 && records[0].Files.Count > 0)
				{
					logger.LogInformation("Deleting shelved files in changelist {Change}", pendingChange.Number);
					await perforce.DeleteChangeAsync(DeleteChangeOptions.None, pendingChange.Number, cancellationToken);
				}

				// delete the changelist
				logger.LogInformation("Deleting changelist {Change}", pendingChange.Number);
				await perforce.DeleteChangeAsync(DeleteChangeOptions.None, pendingChange.Number, cancellationToken);
			}
		}
	}
}
