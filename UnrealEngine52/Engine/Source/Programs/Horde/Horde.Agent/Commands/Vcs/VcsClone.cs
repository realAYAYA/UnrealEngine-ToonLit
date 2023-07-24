// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Commands.Vcs
{
	[Command("vcs", "clone", "Initialize a directory for VCS-like operations")]
	class VcsClone : VcsCheckout
	{
		public VcsClone(IStorageClientFactory storageClientFactory)
			: base(storageClientFactory)
		{
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			BaseDir ??= DirectoryReference.GetCurrentDirectory();
			await InitAsync(BaseDir);
			logger.LogInformation("Initialized in {RootDir}", BaseDir);

			WorkspaceState workspaceState = new WorkspaceState();

			RefName branchName = new RefName(Branch ?? "ue5-main");

			IStorageClient store = await GetStorageClientAsync();

			using MemoryCache cache = new MemoryCache(new MemoryCacheOptions());
			TreeReader reader = new TreeReader(store, cache, logger);

			CommitNode? tip = await GetCommitAsync(reader, branchName, Change);
			if (tip == null)
			{
				logger.LogError("Unable to find change {Change}", Change);
				return 1;
			}

			workspaceState.Tree = await RealizeAsync(reader, tip.Contents, BaseDir, null, true, logger);
			await WriteStateAsync(BaseDir, workspaceState);

			return 0;
		}
	}
}
