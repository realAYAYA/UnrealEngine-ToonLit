// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Compute;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Agent.Commands
{
	/// <summary>
	/// Installs the agent as a service
	/// </summary>
	[Command("ComputeV2", "Executes a command through the Horde Compute API")]
	class ComputeCommandV2 : Command
	{
		[CommandLine("-Cluster=", Description = "Cluster to execute on")]
		public ClusterId ClusterId { get; set; } = new ClusterId("default");

		[CommandLine("-Condition=", Description = "Match the agent to run on")]
		public string? Condition { get; set; }

		readonly AgentSettings _settings;

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeCommandV2(IOptions<AgentSettings> settings)
		{
			_settings = settings.Value;
		}

		/// <inheritdoc/>
		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			ServerProfile profile = _settings.GetCurrentServerProfile();
			logger.LogInformation("Connecting to server: {Server}", profile.Name);

			await using ComputeClient client = new ComputeClient(CreateHttpClient, logger);
			client.Start();

			IComputeRequest<object?> request = await client.AddRequestAsync(ClusterId, null, TestCommandAsync);
			await request.Result;

			return 0;
		}

		HttpClient CreateHttpClient()
		{
			HttpClient client = new HttpClient();
			client.BaseAddress = _settings.GetCurrentServerProfile().Url;
			return client;
		}

		async Task<object?> TestCommandAsync(IComputeChannel channel, CancellationToken cancellationToken)
		{
			await channel.WriteAsync(new XorRequestMessage { Value = 123, Payload = new byte[] { 1, 2, 3, 4, 5 } }, cancellationToken);

			XorResponseMessage response = await channel.ReadAsync<XorResponseMessage>(cancellationToken);

			await channel.WriteAsync(new CloseMessage(), cancellationToken);
			return null;
		}
	}
}
