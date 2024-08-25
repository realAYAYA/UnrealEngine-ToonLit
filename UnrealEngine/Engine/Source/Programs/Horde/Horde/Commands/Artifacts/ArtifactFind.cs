// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Artifacts;
using Microsoft.Extensions.Logging;

namespace Horde.Commands.Bundles
{
	[Command("artifact", "find", "Finds artifacts matching the given query parameters")]
	class ArtifactFind : Command
	{
		[CommandLine("-Key=")]
		[Description("Artifact keys to search for. Multiple keys may be added to artifacts at upload time, eg. 'job:63dd5487c67f8a45453361c5/step:62ce'.")]
		public List<string> Keys { get; } = new List<string>();

		readonly HordeHttpClient _hordeHttpClient;

		public ArtifactFind(HordeHttpClient httpClient)
		{
			_hordeHttpClient = httpClient;
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			List<GetArtifactResponse> artifacts = await _hordeHttpClient.FindArtifactsAsync(keys: Keys);
			foreach (GetArtifactResponse artifact in artifacts)
			{
				logger.LogInformation("");
				logger.LogInformation("Artifact {Id} ({Type})", artifact.Id, artifact.Type);
				foreach (string key in artifact.Keys)
				{
					logger.LogInformation("  {Key}", key);
				}
			}

			return 0;
		}
	}
}
