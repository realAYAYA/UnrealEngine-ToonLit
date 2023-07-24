// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Compute;
using Horde.Agent.Services;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Leases.Handlers
{
	class ComputeHandlerV2 : LeaseHandler<ComputeTaskMessageV2>
	{
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeHandlerV2(ILogger<ComputeHandlerV2> logger)
		{
			_logger = logger;
		}

		/// <inheritdoc/>
		public override async Task<LeaseResult> ExecuteAsync(ISession session, string leaseId, ComputeTaskMessageV2 computeTask, CancellationToken cancellationToken)
		{
			_logger.LogInformation("Starting compute task (lease {LeaseId})", leaseId);

			try
			{
				await ConnectAsync(computeTask, cancellationToken);
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Exception while executing compute task");
			}

			return LeaseResult.Success;
		}

		async Task ConnectAsync(ComputeTaskMessageV2 computeTask, CancellationToken cancellationToken)
		{
			using TcpClient tcpClient = new TcpClient();
			await tcpClient.ConnectAsync(computeTask.RemoteIp, computeTask.RemotePort);

			Socket socket = tcpClient.Client;
			await socket.SendAsync(computeTask.Nonce.Memory, SocketFlags.None, cancellationToken);

			IComputeChannel channel = new SocketComputeChannel(socket, computeTask.AesKey.Memory, computeTask.AesIv.Memory);
			for (; ; )
			{
				object request = await channel.ReadAsync(cancellationToken);
				_logger.LogInformation("Received message {Type}", request.GetType().Name);

				switch (request)
				{
					case CloseMessage _:
						return;
					case XorRequestMessage xorRequest:
						{
							XorResponseMessage response = new XorResponseMessage();
							response.Payload = new byte[xorRequest.Payload.Length];
							for (int idx = 0; idx < xorRequest.Payload.Length; idx++)
							{
								response.Payload[idx] = (byte)(xorRequest.Payload[idx] ^ xorRequest.Value);
							}
							await channel.WriteAsync(response, cancellationToken);
						}
						break;
				}
			}
		}
	}
}

