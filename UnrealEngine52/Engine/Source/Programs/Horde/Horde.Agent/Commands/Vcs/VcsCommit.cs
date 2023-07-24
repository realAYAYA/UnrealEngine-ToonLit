// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Commands.Vcs
{
	[Command("vcs", "commit", "Commits data to the VCS store")]
	class VcsCommit : VcsBase
	{
		[CommandLine("-User=")]
		public string UserName { get; set; } = Environment.UserName;

		[CommandLine("-Message=")]
		public string? Message { get; set; }

		public VcsCommit(IStorageClientFactory storageClientFactory)
			: base(storageClientFactory)
		{
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			DirectoryReference rootDir = FindRootDir();

			WorkspaceState workspaceState = await ReadStateAsync(rootDir);

			DirectoryState oldState = workspaceState.Tree;
			DirectoryState newState = await GetCurrentDirectoryState(rootDir, oldState);

			if (oldState == newState)
			{
				logger.LogInformation("No files modified");
				return 0;
			}

			if (Message == null)
			{
				logger.LogError("Missing -Message=... argument");
				return 1;
			}

			PrintDelta(oldState, newState, logger);

			IStorageClient store = await GetStorageClientAsync();

			using MemoryCache cache = new MemoryCache(new MemoryCacheOptions());
			TreeReader reader = new TreeReader(store, cache, logger);

			CommitNode? tip = await reader.TryReadNodeAsync<CommitNode>(workspaceState.Branch);
			TreeNodeRef<CommitNode>? tipRef = (tip == null) ? null : new TreeNodeRef<CommitNode>(tip);

			DirectoryNode rootNode;
			if (tip == null)
			{
				rootNode = new DirectoryNode();
			}
			else
			{
				rootNode = await tip.Contents.ExpandCopyAsync(reader);
			}

			DirectoryNodeRef rootRef = new DirectoryNodeRef(rootNode);

			List<(DirectoryNode, FileInfo, FileState)> files = new List<(DirectoryNode, FileInfo, FileState)>();
			List<(DirectoryNodeRef, DirectoryState)> directories = new List<(DirectoryNodeRef, DirectoryState)>();
			await UpdateTreeAsync(reader, rootRef, rootDir, oldState, newState, files, directories);

			using TreeWriter writer = new TreeWriter(store);
			await DirectoryNode.CopyFromDirectoryAsync(files.ConvertAll(x => (x.Item1, x.Item2)), new ChunkingOptions(), writer, CancellationToken.None);

			CommitNode newTip;
			if (tip == null)
			{
				newTip = new CommitNode(1, null, UserName, Message, DateTime.UtcNow, rootRef);
			}
			else
			{
				newTip = new CommitNode(tip.Number + 1, tipRef, UserName, Message, DateTime.UtcNow, rootRef);
			}
			await store.WriteNodeAsync(workspaceState.Branch, newTip);

			foreach ((DirectoryNodeRef directoryRef, DirectoryState directoryState) in directories)
			{
				directoryState.Hash = directoryRef.Hash;
			}

			foreach ((DirectoryNode directoryNode, FileInfo fileInfo, FileState fileState) in files)
			{
				FileEntry fileEntry = directoryNode.GetFileEntry(fileInfo.Name);
				fileState.Hash = fileEntry.Hash;
			}

			workspaceState.Change = newTip.Number;
			workspaceState.Tree = newState;
			await WriteStateAsync(rootDir, workspaceState);

			logger.LogInformation("Commited in change {Number}", newTip.Number);
			return 0;
		}

		private async Task UpdateTreeAsync(TreeReader reader, DirectoryNodeRef rootRef, DirectoryReference rootDir, DirectoryState? oldState, DirectoryState newState, List<(DirectoryNode, FileInfo, FileState)> files, List<(DirectoryNodeRef, DirectoryState)> directories)
		{
			directories.Add((rootRef, newState));

			DirectoryNode root = await rootRef.ExpandAsync(reader);
			foreach ((Utf8String name, DirectoryState? oldSubDirState, DirectoryState? newSubDirState) in EnumerableExtensions.Zip(oldState?.Directories, newState.Directories))
			{
				if (newSubDirState == null)
				{
					root.DeleteDirectory(name);
				}
				else if (oldSubDirState != newSubDirState)
				{
					await UpdateTreeAsync(reader, root.FindOrAddDirectoryEntry(name), DirectoryReference.Combine(rootDir, name.ToString()), oldSubDirState, newSubDirState, files, directories);
				}
			}

			foreach ((Utf8String name, FileState? oldFileState, FileState? newFileState) in EnumerableExtensions.Zip(oldState?.Files, newState.Files))
			{
				if (newFileState == null)
				{
					root.DeleteFile(name);
				}
				else if (oldFileState != newFileState)
				{
					files.Add((root, FileReference.Combine(rootDir, name.ToString()).ToFileInfo(), newFileState));
				}
			}
		}
	}
}
