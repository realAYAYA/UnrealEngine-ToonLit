// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using EpicGames.Core;
using EpicGames.Perforce;
using EpicGames.Perforce.Managed;
using Microsoft.Extensions.Logging;

namespace Horde.Commands.Perforce
{
	[Command("perforce", "purgecache", "Shrink the size of the cache to the given size")]
	class PerforcePurge : PerforceBase
	{
		[CommandLine("-Size=")]
		[Description("Maximum size of the cache")]
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
