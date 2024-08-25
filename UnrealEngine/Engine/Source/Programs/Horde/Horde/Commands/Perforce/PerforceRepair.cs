// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Perforce;
using EpicGames.Perforce.Managed;
using Microsoft.Extensions.Logging;

namespace Horde.Commands.Perforce
{
	[Command("perforce", "repaircache", "Checks the integrity of the cache, and removes any invalid files")]
	class PerforceRepair : PerforceBase
	{
		protected override Task ExecuteAsync(IPerforceConnection perforce, ManagedWorkspace repo, ILogger logger)
		{
			return repo.RepairAsync(CancellationToken.None);
		}
	}
}
