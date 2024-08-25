// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Storage;
using Microsoft.Extensions.Logging;

namespace Horde.Commands.Vcs
{
	[Command("vcs", "init", "Initialize a directory for VCS-like operations", Advertise = false)]
	class VcsInit : VcsBase
	{
		public VcsInit(IStorageClientFactory storageClientFactory)
			: base(storageClientFactory)
		{
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			BaseDir ??= DirectoryReference.GetCurrentDirectory();
			await InitAsync(BaseDir);
			logger.LogInformation("Initialized in {RootDir}", BaseDir);
			return 0;
		}
	}
}
