// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Commands
{
	[Command("config", "Updates the configuration for the Horde tool")]
	class ConfigCommand : Command
	{
		[CommandLine("-Server=")]
		[Description("Updates the server URL")]
		public string? Server { get; set; }

		readonly CmdConfig _config;

		public ConfigCommand(IOptions<CmdConfig> config)
		{
			_config = config.Value;
		}

		/// <inheritdoc/>
		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			if (Server != null)
			{
				_config.Server = new Uri(Server);
				await _config.WriteAsync();
			}

			logger.LogInformation("Server: {Server}", _config.Server);
			return 0;
		}
	}
}
