// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Perforce;
using EpicGames.Perforce.Managed;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Commands.Workspace
{
	[Command("Workspace", "Dump", "Dumps the contents of the repository to the log for analysis")]
	class Dump : WorkspaceCommand
	{
		protected override Task ExecuteAsync(IPerforceConnection perforce, ManagedWorkspace repo, ILogger logger)
		{
			repo.Dump();
			return Task.CompletedTask;
		}
	}
}
