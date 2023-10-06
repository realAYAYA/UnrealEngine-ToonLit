// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace Horde.Commands
{
	[Command("config", "Updates the configuration for the Horde tool")]
	class ConfigCommand : Command
	{
		[CommandLine("-Server=", Description = "Updates the server URL")]
		public string? Server { get; set; }

		/// <inheritdoc/>
		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			if (Server != null)
			{
				await Settings.SetServerAsync(Server);
			}

			logger.LogInformation("Server: {Server}", await Settings.GetServerAsync());
			return 0;
		}
	}
}
