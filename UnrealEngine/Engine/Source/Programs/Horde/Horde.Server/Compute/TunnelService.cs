// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using EpicGames.Core;
using EpicGames.Horde.Compute;
using System.IO;
using System.Buffers;
using System.Buffers.Binary;
using EpicGames.Horde.Compute.Clients;

namespace Horde.Server.Compute
{
	/// <summary>
	/// Tunnels connections between Horde agents and compute initiators
	/// </summary>
	public sealed class TunnelService : IHostedService, IDisposable
	{
		const int BufferSize = 4096;

		class Tunnel
		{
			public Socket InitiatorSocket { get; }
			public TaskCompletionSource<Socket> RemoteSocket { get; } = new TaskCompletionSource<Socket>();
			public AsyncEvent Complete { get; } = new AsyncEvent();

			public Tunnel(Socket initiatorSocket) => InitiatorSocket = initiatorSocket;
		}

		readonly CancellationTokenSource _cancellationSource;
		readonly ServerSettings _settings;
		readonly ILogger _logger;

		Task? _initiatorServerTask;
		Task? _remoteServerTask;

		readonly Dictionary<ByteString, Tunnel> _waiters = new Dictionary<ByteString, Tunnel>();

		/// <summary>
		/// Constructor
		/// </summary>
		public TunnelService(IOptions<ServerSettings> settings, ILogger<TunnelService> logger)
		{
			_cancellationSource = new CancellationTokenSource();
			_settings = settings.Value;
			_logger = logger;
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_cancellationSource.Dispose();
		}

		/// <inheritdoc/>
		public Task StartAsync(CancellationToken cancellationToken)
		{
			Start(IPAddress.Any);
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public void Start(IPAddress address)
		{
			_initiatorServerTask = RunTcpListenerAsync(address, _settings.ComputeInitiatorPort, HandleInitiatorAsync, _cancellationSource.Token);
			_remoteServerTask = RunTcpListenerAsync(address, _settings.ComputeRemotePort, HandleRemoteAsync, _cancellationSource.Token);
		}

		/// <inheritdoc/>
		public async Task StopAsync(CancellationToken cancellationToken)
		{
			_cancellationSource.Cancel();

			await _initiatorServerTask!.IgnoreCanceledExceptionsAsync().ConfigureAwait(false);
			_initiatorServerTask = null;

			await _remoteServerTask!.IgnoreCanceledExceptionsAsync().ConfigureAwait(false);
			_remoteServerTask = null;
		}

		async Task HandleRemoteAsync(Socket remoteSocket, CancellationToken cancellationToken)
		{
			// Read the nonce for the connection
			byte[] nonceBuffer = new byte[ServerComputeClient.NonceLength];
			await ReadFullBufferAsync(remoteSocket, nonceBuffer, cancellationToken);
			ByteString nonce = new ByteString(nonceBuffer);

			// Get the completion source for this nonce
			Tunnel? tunnel;
			lock (_waiters)
			{
				_waiters.TryGetValue(nonce, out tunnel);
			}

			// Pump data from the remote socket through to the initiator socket
			if (tunnel != null && tunnel.RemoteSocket.TrySetResult(remoteSocket))
			{
				await tunnel.Complete.Task;
			}
		}

		async Task HandleInitiatorAsync(Socket initiatorSocket, CancellationToken cancellationToken)
		{
			// Read the nonce for the connection
			byte[] nonceBuffer = new byte[ServerComputeClient.NonceLength];
			await ReadFullBufferAsync(initiatorSocket, nonceBuffer, cancellationToken);
			ByteString nonce = new ByteString(nonceBuffer);

			// Create the tunnel and add it to the waiting list
			Tunnel tunnel = new Tunnel(initiatorSocket);
			lock (_waiters)
			{
				_waiters.Add(nonce, tunnel);
			}

			// Reply with the port number that the remote should connect on
			byte[] ackBuffer = new byte[2];
			BinaryPrimitives.WriteUInt16LittleEndian(ackBuffer, (ushort)_settings.ComputeRemotePort);
			await SendFullBufferAsync(initiatorSocket, ackBuffer, cancellationToken);

			// Try/finally to mark the tunnel as complete when finished
			try
			{
				// Start a read from the initiating socket, so we can react to disconnection events
				using IMemoryOwner<byte> initiatorToRemoteBuffer = MemoryPool<byte>.Shared.Rent(BufferSize);
				Task<int> readTask = initiatorSocket.ReceiveAsync(initiatorToRemoteBuffer.Memory, SocketFlags.None, cancellationToken).AsTask();

				// Wait for the other end of the socket to be available
				Task initTask = Task.WhenAny(readTask, tunnel.RemoteSocket.Task);
				if (initTask == readTask)
				{
					return;
				}

				// Start a task to copy data from the remote back to the initiator
				Socket remoteSocket = await tunnel.RemoteSocket.Task;

				// Start copying data from the remote to the initiator
				using IMemoryOwner<byte> remoteToInitiatorBuffer = MemoryPool<byte>.Shared.Rent(BufferSize);
				Task remoteToInitiatorTask = CopySocketDataAsync(remoteSocket, tunnel.InitiatorSocket, remoteToInitiatorBuffer.Memory, cancellationToken);

				// Copy data from the initiator to the remote
				int readSize = await readTask;
				await SendFullBufferAsync(remoteSocket, initiatorToRemoteBuffer.Memory.Slice(0, readSize), cancellationToken);
				await CopySocketDataAsync(initiatorSocket, remoteSocket, initiatorToRemoteBuffer.Memory, cancellationToken);

				// Wait for the socket to close
				await remoteToInitiatorTask;
			}
			finally
			{
				// Remove the nonce to the waiting list
				lock (_waiters)
				{
					_waiters.Remove(nonce);
				}

				// Signal to the other end of the socket that we're complete
				tunnel.RemoteSocket.TrySetResult(null!);
				tunnel.Complete.Latch();
			}
		}

		static async Task CopySocketDataAsync(Socket sourceSocket, Socket targetSocket, Memory<byte> buffer, CancellationToken cancellationToken)
		{
			for (; ; )
			{
				int read = await sourceSocket.ReceiveAsync(buffer, SocketFlags.None, cancellationToken);
				if (read == 0)
				{
					break;
				}
				await SendFullBufferAsync(targetSocket, buffer.Slice(0, read), cancellationToken);
			}
		}

		static async Task ReadFullBufferAsync(Socket socket, Memory<byte> buffer, CancellationToken cancellationToken)
		{
			for (int offset = 0; offset < buffer.Length; )
			{
				int read = await socket.ReceiveAsync(buffer.Slice(offset), SocketFlags.None, cancellationToken);
				if (read == 0)
				{
					throw new EndOfStreamException();
				}
				offset += read;
			}
		}

		static async Task SendFullBufferAsync(Socket socket, ReadOnlyMemory<byte> buffer, CancellationToken cancellationToken)
		{
			while (buffer.Length > 0)
			{
				int written = await socket.SendAsync(buffer, SocketFlags.None, cancellationToken);
				buffer = buffer.Slice(written);
			}
		}

		async Task RunTcpListenerAsync(IPAddress address, int port, Func<Socket, CancellationToken, Task> handleConnectionAsync, CancellationToken cancellationToken)
		{
			if (port != 0)
			{
				TcpListener listener = new TcpListener(address, port);
				listener.Start();

				List<Task> tasks = new List<Task>();
				try
				{
					for (; ; )
					{
						TcpClient client = await listener.AcceptTcpClientAsync(cancellationToken);
						_logger.LogInformation("Received connection from {Remote}", client.Client.RemoteEndPoint);

						Task task = HandleConnectionGuardedAsync(client, handleConnectionAsync, cancellationToken);
						tasks.Add(task);
					}
				}
				finally
				{
					await Task.WhenAll(tasks);
					listener.Stop();
				}
			}
		}

		async Task HandleConnectionGuardedAsync(TcpClient tcpClient, Func<Socket, CancellationToken, Task> handleConnectionAsync, CancellationToken cancellationToken)
		{
			try
			{
				await handleConnectionAsync(tcpClient.Client, cancellationToken);
			}
			catch (Exception ex)
			{
				_logger.LogInformation(ex, "Exception while handling request: {Message}", ex.Message);
			}
			finally
			{
				tcpClient.Dispose();
			}
		}
	}
}
