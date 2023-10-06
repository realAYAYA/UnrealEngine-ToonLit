// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Compute.Transports;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Compute.Clients
{
	/// <summary>
	/// Implementation of <see cref="IComputeClient"/> which marshals data over a loopback connection to a method running on a background task in the same process.
	/// </summary>
	public sealed class LocalComputeClient : IComputeClient
	{
		class LeaseImpl : IComputeLease
		{
			public IReadOnlyList<string> Properties { get; } = new List<string>();
			public IReadOnlyDictionary<string, int> AssignedResources => new Dictionary<string, int>();
			public ComputeSocket Socket => _socket;

			readonly ComputeSocket _socket;

			public LeaseImpl(ComputeSocket socket) => _socket = socket;

			/// <inheritdoc/>
			public ValueTask DisposeAsync() => _socket.DisposeAsync();

			/// <inheritdoc/>
			public ValueTask CloseAsync(CancellationToken cancellationToken) => _socket.CloseAsync(cancellationToken);
		}

		readonly BackgroundTask _listenerTask;
		readonly Socket _listener;
		readonly Socket _socket;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="port">Port to connect on</param>
		/// <param name="sandboxDir">Sandbox directory for the worker</param>
		/// <param name="logger">Logger for diagnostic output</param>
		public LocalComputeClient(int port, DirectoryReference sandboxDir, ILogger logger)
		{
			_logger = logger;

			_listener = new Socket(SocketType.Stream, ProtocolType.IP);
			_listener.Bind(new IPEndPoint(IPAddress.Loopback, port));
			_listener.Listen();

			_listenerTask = BackgroundTask.StartNew(ctx => RunListenerAsync(_listener, sandboxDir, logger, ctx));

			_socket = new Socket(SocketType.Stream, ProtocolType.IP);
			_socket.Connect(IPAddress.Loopback, port);
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			_socket.Dispose();
			await _listenerTask.DisposeAsync();
			_listener.Dispose();
		}

		/// <summary>
		/// Sets up the loopback listener and calls the server method
		/// </summary>
		static async Task RunListenerAsync(Socket listener, DirectoryReference sandboxDir, ILogger logger, CancellationToken cancellationToken)
		{
			using Socket tcpSocket = await listener.AcceptAsync(cancellationToken);

			using MemoryCache memoryCache = new MemoryCache(new MemoryCacheOptions { SizeLimit = 10 * 1024 * 1024 });

			await using (ComputeSocket socket = new ComputeSocket(new TcpTransport(tcpSocket), ComputeSocketEndpoint.Remote, logger))
			{
				AgentMessageHandler worker = new AgentMessageHandler(sandboxDir, memoryCache, logger);
				await worker.RunAsync(socket, cancellationToken);
				await socket.CloseAsync(cancellationToken);
			}
		}

		/// <inheritdoc/>
		public Task<IComputeLease?> TryAssignWorkerAsync(ClusterId clusterId, Requirements? requirements, CancellationToken cancellationToken)
		{
#pragma warning disable CA2000 // Dispose objects before losing scope
			ComputeSocket socket = new ComputeSocket(new TcpTransport(_socket), ComputeSocketEndpoint.Local, _logger);
			return Task.FromResult<IComputeLease?>(new LeaseImpl(socket));
#pragma warning restore CA2000 // Dispose objects before losing scope
		}
	}
}
