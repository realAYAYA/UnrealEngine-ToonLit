// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Perforce;
using EpicGames.Perforce.Managed;
using Microsoft.Extensions.Logging;

namespace Horde.Commands.Workspace
{
	[Command("workspace", "repaircache", "Checks the integrity of the cache, and removes any invalid files")]
	class WorkspaceRepair : WorkspaceBase
	{
		protected override Task ExecuteAsync(IPerforceConnection perforce, ManagedWorkspace repo, ILogger logger)
		{
			return repo.RepairAsync(CancellationToken.None);
		}
	}
}
