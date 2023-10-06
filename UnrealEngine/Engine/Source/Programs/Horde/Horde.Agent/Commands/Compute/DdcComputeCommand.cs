// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Compute;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Commands.Compute
{
	/// <summary>
	/// Installs the agent as a service
	/// </summary>
	[Command("ddccompute", "Executes a command through the Horde Compute API")]
	class DdcComputeCommand : ComputeCommand
	{
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public DdcComputeCommand(IServiceProvider serviceProvider, ILogger<DdcComputeCommand> logger)
			: base(serviceProvider)
		{
			_logger = logger;
		}

		/// <inheritdoc/>
		protected override async Task<bool> HandleRequestAsync(IComputeLease lease, CancellationToken cancellationToken)
		{
			IComputeSocket socket = lease.Socket;
			using (AgentMessageChannel channel = socket.CreateAgentMessageChannel(0, _logger))
			{
				_logger.LogInformation("Sending XOR request");
				await channel.SendXorRequestAsync(new byte[] { 1, 2, 3, 4, 5 }, (byte)123, cancellationToken);

				_logger.LogInformation("Waiting for response...");
				AgentMessage response = await channel.ReceiveAsync(cancellationToken);

				byte[] result = response.Data.ToArray();
				byte[] expectedResult = new byte[] { 1 ^ 123, 2 ^ 123, 3 ^ 123, 4 ^ 123, 5 ^ 123 };

				if (result.SequenceEqual(expectedResult))
				{
					_logger.LogInformation("Received response; data is correct.");
				}
				else
				{
					throw new Exception("Incorrect response data");
				}

				_logger.LogInformation("Closing channel");
			}

			_logger.LogInformation("Closed");
			return true;
		}
	}
}
