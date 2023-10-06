// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Compute.Transports
{
	/// <summary>
	/// Implementation of <see cref="IComputeTransport"/> for communicating over a socket
	/// </summary>
	public sealed class TcpTransport : IComputeTransport
	{
		readonly Socket _socket;

		/// <inheritdoc/>
		public long Position { get; private set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="socket">Socket to communicate over</param>
		public TcpTransport(Socket socket) => _socket = socket;

		/// <inheritdoc/>
		public async ValueTask<int> ReadPartialAsync(Memory<byte> buffer, CancellationToken cancellationToken)
		{
			int read = await _socket.ReceiveAsync(buffer, SocketFlags.None, cancellationToken);
			Position += read;
			return read;
		}

		/// <inheritdoc/>
		public async ValueTask WriteAsync(ReadOnlySequence<byte> buffer, CancellationToken cancellationToken)
		{
			foreach (ReadOnlyMemory<byte> memory in buffer)
			{
				await _socket.SendMessageAsync(memory, SocketFlags.None, cancellationToken);
				Position += memory.Length;
			}
		}

		/// <inheritdoc/>
		public ValueTask MarkCompleteAsync(CancellationToken cancelationToken)
		{
			_socket.Shutdown(SocketShutdown.Send);
			return new ValueTask();
		}
	}
}
