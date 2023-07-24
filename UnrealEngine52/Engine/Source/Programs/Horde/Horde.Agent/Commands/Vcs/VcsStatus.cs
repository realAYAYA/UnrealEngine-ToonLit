// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Commands.Vcs
{
	[Command("Vcs", "Status", "Find status of local files")]
	class VcsStatus : VcsBase
	{
		public VcsStatus(IStorageClientFactory storageClientFactory)
			: base(storageClientFactory)
		{
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			DirectoryReference rootDir = FindRootDir();

			WorkspaceState workspaceState = await ReadStateAsync(rootDir);

			DirectoryState oldState = workspaceState.Tree;
			DirectoryState newState = await GetCurrentDirectoryState(rootDir, oldState);

			logger.LogInformation("On branch {BranchName}", workspaceState.Branch);

			if (oldState == newState)
			{
				logger.LogInformation("No files modified");
			}
			else
			{
				PrintDelta(oldState, newState, logger);
			}

			return 0;
		}
	}
}
