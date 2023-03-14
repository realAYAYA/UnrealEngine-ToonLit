// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Agent.Services;
using HordeCommon.Rpc.Messages;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Commands.Utilities
{
	/// <summary>
	/// Shows capabilities of this agent
	/// </summary>
	[Command("Caps", "Lists detected capabilities of this agent")]
	class CapsCommand : Command
	{
		/// <inheritdoc/>
		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			AgentCapabilities capabilities = await WorkerService.GetAgentCapabilities(DirectoryReference.GetCurrentDirectory(), logger);
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
