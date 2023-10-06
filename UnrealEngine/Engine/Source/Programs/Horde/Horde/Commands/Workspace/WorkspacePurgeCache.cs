// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Perforce;
using EpicGames.Perforce.Managed;
using Microsoft.Extensions.Logging;

namespace Horde.Commands.Workspace
{
	[Command("workspace", "purgecache", "Shrink the size of the cache to the given size")]
	class WorkspacePurgeCache : WorkspaceBase
	{
		[CommandLine("-Size=")]
		string? SizeParam { get; set; } = null;

		protected override Task ExecuteAsync(IPerforceConnection perforce, ManagedWorkspace repo, ILogger logger)
		{
			long size = 0;
			if (SizeParam != null)
			{
				size = ParseSize(SizeParam);
			}

			return repo.PurgeAsync(size, CancellationToken.None);
		}
	}
}
