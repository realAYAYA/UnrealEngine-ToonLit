// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Perforce;
using EpicGames.Perforce.Managed;
using Microsoft.Extensions.Logging;

namespace Horde.Commands.Workspace
{
	[Command("workspace", "clear", "Empties the staging directory of any files, returning them to the cache")]
	class WorkspaceClear : WorkspaceBase
	{
		protected override Task ExecuteAsync(IPerforceConnection perforce, ManagedWorkspace repo, ILogger logger)
		{
			return repo.ClearAsync(CancellationToken.None);
		}
	}
}
