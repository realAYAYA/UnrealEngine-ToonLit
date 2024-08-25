// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using System.Diagnostics;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Clients;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Commands.Workspace
{
	using Workspace = EpicGames.Horde.Storage.Workspace;

	[Command("workspace", "sync", "Extracts an archive into the workspace")]
	class WorkspaceSync : StorageCommandBase
	{
		[CommandLine("-Root=")]
		[Description("Root directory for the managed workspace.")]
		public DirectoryReference? RootDir { get; set; }

		[CommandLine("-File=")]
		[Description("Path to a text file containing the node to extract to this workspace.")]
		public FileReference? File { get; set; }

		[CommandLine("-Ref=")]
		[Description("Name of a ref to extract to this workspace.")]
		public string? Ref { get; set; }

		[CommandLine("-Node=")]
		[Description("Locator for a node to extract to this workspace.")]
		public string? Node { get; set; }

		[CommandLine("-Layer=")]
		[Description("Name of the layer to extract to.")]
		public WorkspaceLayerId LayerId { get; set; } = WorkspaceLayerId.Default;

		[CommandLine("-Stats")]
		[Description("Outputs stats for the extraction operation.")]
		public bool Stats { get; set; }

		public WorkspaceSync(HttpStorageClientFactory storageClientFactory, BundleCache bundleCache, IOptions<CmdConfig> config)
			: base(storageClientFactory, bundleCache, config)
		{
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			if (File != null)
			{
				using IStorageClient store = BundleStorageClient.CreateFromDirectory(File.Directory, BundleCache, logger);
				IBlobHandle handle = store.CreateBlobHandle(await FileStorageBackend.ReadRefAsync(File));
				return await ExecuteInternalAsync(store, handle, logger);
			}
			else if (Ref != null)
			{
				using IStorageClient store = CreateStorageClient();
				IBlobHandle handle = await store.ReadRefAsync(new RefName(Ref));
				return await ExecuteInternalAsync(store, handle, logger);
			}
			else if (Node != null)
			{
				using IStorageClient store = CreateStorageClient();
				IBlobHandle handle = store.CreateBlobHandle(new BlobLocator(Node));
				return await ExecuteInternalAsync(store, handle, logger);
			}
			else
			{
				throw new CommandLineArgumentException("Either -File=... or -Ref=... must be specified");
			}
		}

		async Task<int> ExecuteInternalAsync(IStorageClient store, IBlobHandle handle, ILogger logger)
		{
			RootDir ??= DirectoryReference.GetCurrentDirectory();
			CancellationToken cancellationToken = CancellationToken.None;

			Workspace? workspace = await Workspace.TryOpenAsync(RootDir, logger, cancellationToken);
			if (workspace == null)
			{
				logger.LogError("No workspace has been initialized in {RootDir}. Use 'workspace init' to create a new workspace.", RootDir);
				return 1;
			}

			Stopwatch timer = Stopwatch.StartNew();
			logger.LogInformation("Syncing into layer '{LayerId}'...", LayerId);

			DirectoryNode contents = await handle.ReadBlobAsync<DirectoryNode>();
			await workspace.SyncAsync(LayerId, contents, cancellationToken);
			await workspace.SaveAsync(cancellationToken);

			logger.LogInformation("Elapsed: {Time}s", timer.Elapsed.TotalSeconds);

			if (Stats)
			{
				StorageStats stats = store.GetStats();
				stats.Print(logger);
			}

			return 0;
		}
	}
}
