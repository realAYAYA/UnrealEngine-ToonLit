// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using Horde.Agent.Services;
using HordeCommon.Rpc.Messages;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Commands.Utilities
{
	/// <summary>
	/// Shows capabilities of this agent
	/// </summary>
	[Command("caps", "Lists detected capabilities of this agent")]
	class CapsCommand : Command
	{
		readonly CapabilitiesService _capabilitiesService;

		/// <summary>
		/// Constructor
		/// </summary>
		public CapsCommand(CapabilitiesService capabilitiesService)
		{
			_capabilitiesService = capabilitiesService;
		}

		/// <inheritdoc/>
		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			AgentCapabilities capabilities = await _capabilitiesService.GetCapabilitiesAsync(null);
			foreach (DeviceCapabilities device in capabilities.Devices)
			{
				logger.LogInformation("Device: {Name}", device.Handle);
				foreach (string property in device.Properties)
				{
					logger.LogInformation("  {Property}", property);
				}
			}
			return 0;
		}
	}
}
