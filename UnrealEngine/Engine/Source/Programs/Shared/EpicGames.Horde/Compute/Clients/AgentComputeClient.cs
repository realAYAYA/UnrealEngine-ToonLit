// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Net;
using System.Net.Sockets;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Compute.Transports;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Compute.Clients
{
	/// <summary>
	/// Runs a local Horde Agent process to process compute requests without communicating with a server
	/// </summary>
	public sealed class AgentComputeClient : IComputeClient
	{
		class LeaseImpl : IComputeLease
		{
			readonly IAsyncEnumerator<ComputeSocket> _source;

			/// <inheritdoc/>
			public IReadOnlyList<string> Properties { get; } = new List<string>();

			/// <inheritdoc/>
			public IReadOnlyDictionary<string, int> AssignedResources => new Dictionary<string, int>();

			/// <inheritdoc/>
			public ComputeSocket Socket => _source.Current;

			public LeaseImpl(IAsyncEnumerator<ComputeSocket> source) => _source = source;

			/// <inheritdoc/>
			public async ValueTask DisposeAsync()
			{
				await _source.MoveNextAsync();
				await _source.DisposeAsync();
			}

			/// <inheritdoc/>
			public ValueTask CloseAsync(CancellationToken cancellationToken) => Socket.CloseAsync(cancellationToken);
		}

		readonly string _hordeAgentAssembly;
		readonly int _port;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="hordeAgentAssembly">Path to the Horde Agent assembly</param>
		/// <param name="port">Loopback port to connect on</param>
		/// <param name="logger">Factory for logger instances</param>
		public AgentComputeClient(string hordeAgentAssembly, int port, ILogger logger)
		{
			_hordeAgentAssembly = hordeAgentAssembly;
			_port = port;
			_logger = logger;
		}

		/// <inheritdoc/>
		public ValueTask DisposeAsync() => new ValueTask();

		/// <inheritdoc/>
		public async Task<IComputeLease?> TryAssignWorkerAsync(ClusterId clusterId, Requirements? requirements, CancellationToken cancellationToken)
		{
			_logger.LogInformation("** CLIENT **");
			_logger.LogInformation("Launching {Path} to handle remote", _hordeAgentAssembly);

			// The connection logic is an async enumerator that returns the socket, then shuts down.
			IAsyncEnumerator<ComputeSocket> source = ConnectAsync(cancellationToken).GetAsyncEnumerator(cancellationToken);
			if (!await source.MoveNextAsync())
			{
				await source.DisposeAsync();
				return null;
			}

			return new LeaseImpl(source);
		}

		async IAsyncEnumerable<ComputeSocket> ConnectAsync([EnumeratorCancellation] CancellationToken cancellationToken)
		{
			using Socket listener = new Socket(SocketType.Stream, ProtocolType.IP);
			listener.Bind(new IPEndPoint(IPAddress.Loopback, _port));
			listener.Listen();

			using BackgroundTask agentTask = BackgroundTask.StartNew(ctx => RunAgentAsync(_hordeAgentAssembly, _port, _logger, ctx));
			using Socket tcpSocket = await listener.AcceptAsync(cancellationToken);

			await using ComputeSocket socket = new ComputeSocket(new TcpTransport(tcpSocket), ComputeSocketEndpoint.Local, _logger);
			yield return socket;

			await socket.CloseAsync(cancellationToken);
		}

		static async Task RunAgentAsync(string hordeAgentAssembly, int port, ILogger logger, CancellationToken cancellationToken)
		{
			using ManagedProcessGroup group = new ManagedProcessGroup();
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				List<string> arguments = new List<string>();
				arguments.Add(hordeAgentAssembly);
				arguments.Add("computeworker");
				arguments.Add($"-port={port}");

				using ManagedProcess process = new ManagedProcess(group, "dotnet", CommandLineArguments.Join(arguments), null, null, ProcessPriorityClass.Normal);

				string? line;
				while ((line = await process.ReadLineAsync(cancellationToken)) != null)
				{
					logger.LogInformation("Output: {Line}", line);
				}

				await process.WaitForExitAsync(cancellationToken);
			}
			else
			{
				using Process process = new Process();
				process.StartInfo.FileName = "dotnet";
				process.StartInfo.ArgumentList.Add(hordeAgentAssembly);
				process.StartInfo.ArgumentList.Add("computeworker");
				process.StartInfo.ArgumentList.Add($"-port={port}");
				process.StartInfo.UseShellExecute = true;
				process.Start();

				await process.WaitForExitAsync(cancellationToken);
			}
		}
	}
}
