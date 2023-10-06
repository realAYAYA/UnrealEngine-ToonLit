// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Sockets;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Common;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Compute.Clients;
using EpicGames.Horde.Compute.Transports;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Agent.Commands.Compute
{
	abstract class ComputeCommand : Command
	{
		[CommandLine("-Cluster")]
		public string ClusterId { get; set; } = "default";

		[CommandLine("-Requirements=", Description = "Match the agent to run on")]
		public string? Requirements { get; set; }

		[CommandLine("-Local")]
		public bool Local { get; set; }

		[CommandLine("-Loopback")]
		public bool Loopback { get; set; }

		[CommandLine("-Sandbox=")]
		public DirectoryReference SandboxDir { get; set; } = DirectoryReference.Combine(Program.DataDir, "Sandbox");

		readonly IServiceProvider _serviceProvider;
		readonly IHttpClientFactory _httpClientFactory;
		readonly IOptions<AgentSettings> _settings;

		public ComputeCommand(IServiceProvider serviceProvider)
		{
			_serviceProvider = serviceProvider;
			_httpClientFactory = serviceProvider.GetRequiredService<IHttpClientFactory>();
			_settings = serviceProvider.GetRequiredService<IOptions<AgentSettings>>();
		}

		/// <inheritdoc/>
		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			await using IComputeClient client = CreateClient(_serviceProvider.GetRequiredService<ILogger<IComputeClient>>());

			Requirements? requirements = null;
			if (Requirements != null)
			{
				requirements = new Requirements(Condition.Parse(Requirements));
			}

			await using IComputeLease? lease = await client.TryAssignWorkerAsync(new ClusterId(ClusterId), requirements, CancellationToken.None);
			if (lease == null)
			{
				throw new Exception("Unable to create lease");
			}

			bool result = await HandleRequestAsync(lease, CancellationToken.None);
			return result ? 0 : 1;
		}

		/// <summary>
		/// Callback for executing a request
		/// </summary>
		/// <param name="lease"></param>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		protected abstract Task<bool> HandleRequestAsync(IComputeLease lease, CancellationToken cancellationToken);

		IComputeClient CreateClient(ILogger logger)
		{
			if (Local)
			{
				return new LocalComputeClient(2000, SandboxDir, logger);
			}
			else if (Loopback)
			{
				return new AgentComputeClient(Assembly.GetExecutingAssembly().Location, 2000, logger);
			}
			else
			{
				return new ServerComputeClient(CreateHttpClient, logger);
			}
		}

		HttpClient CreateHttpClient()
		{
			ServerProfile profile = _settings.Value.GetCurrentServerProfile();

			HttpClient client = _httpClientFactory.CreateClient();
			client.BaseAddress = profile.Url;
			client.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", profile.Token);
			return client;
		}
	}

	/// <summary>
	/// Helper command for hosting a local compute worker in a separate process
	/// </summary>
	[Command("computeworker", "Runs the agent as a local compute host, accepting incoming connections on the loopback adapter with a given port")]
	class ComputeWorkerCommand : Command
	{
		readonly IMemoryCache _memoryCache;

		[CommandLine("-Port=")]
		int Port { get; set; } = 2000;

		public ComputeWorkerCommand(IMemoryCache memoryCache)
		{
			_memoryCache = memoryCache;
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			logger.LogInformation("** WORKER **");

			using Socket tcpSocket = new Socket(SocketType.Stream, ProtocolType.IP);
			await tcpSocket.ConnectAsync(IPAddress.Loopback, Port);

			await using (ComputeSocket socket = new ComputeSocket(new TcpTransport(tcpSocket), ComputeSocketEndpoint.Remote, logger))
			{
				logger.LogInformation("Running worker...");
				await RunWorkerAsync(socket, _memoryCache, logger, CancellationToken.None);
				logger.LogInformation("Worker complete");
				await socket.CloseAsync(CancellationToken.None);
			}

			logger.LogInformation("Stopping");
			return 0;
		}

		public static async Task RunWorkerAsync(IComputeSocket socket, IMemoryCache memoryCache, ILogger logger, CancellationToken cancellationToken)
		{
			DirectoryReference sandboxDir = DirectoryReference.Combine(Program.DataDir, "Sandbox");

			AgentMessageHandler worker = new AgentMessageHandler(sandboxDir, memoryCache, logger);
			await worker.RunAsync(socket, cancellationToken);
		}
	}
}
