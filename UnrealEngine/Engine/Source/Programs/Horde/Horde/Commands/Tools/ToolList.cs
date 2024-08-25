// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Tools;
using Microsoft.Extensions.Logging;

namespace Horde.Commands
{
	[Command("tool", "list", "List all tools available for download")]
	class ToolList : Command
	{
		readonly HordeHttpClient _hordeHttpClient;

		public ToolList(HordeHttpClient httpClient)
		{
			_hordeHttpClient = httpClient;
		}

		/// <inheritdoc/>
		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			GetToolsSummaryResponse response = await _hordeHttpClient.GetToolsAsync();
			foreach (GetToolSummaryResponse tool in response.Tools)
			{
				logger.LogInformation("  {ToolId,-30} {Deployment,-30} {Version,-15}", tool.Id, tool.DeploymentId, tool.Version);
			}
			return 0;
		}
	}
}
