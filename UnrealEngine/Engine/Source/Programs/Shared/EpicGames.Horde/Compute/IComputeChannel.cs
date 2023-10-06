// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Compute.Buffers;

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Conventional TCP-like interface for writing data to a socket. Sends are "push", receives are "pull".
	/// </summary>
	public interface IComputeChannel : IDisposable
	{
		/// <summary>
		/// Sends data to a remote channel
		/// </summary>
		/// <param name="memory">Memory to write</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		ValueTask SendAsync(ReadOnlyMemory<byte> memory, CancellationToken cancellationToken = default);

		/// <summary>
		/// Marks a channel as complete
		/// </summary>
		/// <param name="buffer">Buffer to receive the data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		ValueTask<int> ReceiveAsync(Memory<byte> buffer, CancellationToken cancellationToken = default);

		/// <summary>
		/// Mark the channel as complete (ie. that no more data will be sent)
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		ValueTask MarkCompleteAsync(CancellationToken cancellationToken = default);
	}

	/// <summary>
	/// Opens a channel for compute workers
	/// </summary>
	public static class ComputeChannel
	{
		class BufferedReaderChannel : IComputeChannel
		{
			readonly ComputeSocket _socket;
			readonly int _channelId;
			readonly IComputeBufferReader _recvBufferReader;

			public BufferedReaderChannel(ComputeSocket socket, int channelId, IComputeBuffer recvBuffer)
			{
				_socket = socket;
				_channelId = channelId;
				_recvBufferReader = recvBuffer.Reader.AddRef();

				_socket.AttachRecvBuffer(_channelId, recvBuffer.Writer);
			}

			public void Dispose()
			{
				_recvBufferReader.Dispose();
			}

			public ValueTask<int> ReceiveAsync(Memory<byte> buffer, CancellationToken cancellationToken = default) => _recvBufferReader.ReadAsync(buffer, cancellationToken);

			public ValueTask SendAsync(ReadOnlyMemory<byte> memory, CancellationToken cancellationToken = default) => _socket.SendAsync(_channelId, memory, cancellationToken);

			public ValueTask MarkCompleteAsync(CancellationToken cancellationToken = default) => _socket.MarkCompleteAsync(_channelId, cancellationToken);
		}

		internal sealed class BufferedReaderWriterChannel : IComputeChannel
		{
			readonly IComputeBufferReader _recvBufferReader;
			readonly IComputeBufferWriter _sendBufferWriter;

			public BufferedReaderWriterChannel(IComputeBufferReader recvBufferReader, IComputeBufferWriter sendBufferWriter)
			{
				_recvBufferReader = recvBufferReader.AddRef();
				_sendBufferWriter = sendBufferWriter.AddRef();
			}

			public void Dispose()
			{
				_recvBufferReader.Dispose();

				_sendBufferWriter.MarkComplete();
				_sendBufferWriter.Dispose();
			}

			public ValueTask<int> ReceiveAsync(Memory<byte> buffer, CancellationToken cancellationToken = default) => _recvBufferReader.ReadAsync(buffer, cancellationToken);

			public ValueTask SendAsync(ReadOnlyMemory<byte> memory, CancellationToken cancellationToken = default) => _sendBufferWriter.WriteAsync(memory, cancellationToken);

			public ValueTask MarkCompleteAsync(CancellationToken cancellationToken = default)
			{
				_sendBufferWriter.MarkComplete();
				return new ValueTask();
			}
		}

		/// <summary>
		/// Creates a channel using a socket and receive buffer
		/// </summary>
		/// <param name="socket">Socket to use for sending data</param>
		/// <param name="channelId">Channel id to send and receive data</param>
		/// <param name="recvBuffer">Buffer for receiving data</param>
		public static IComputeChannel CreateChannel(this ComputeSocket socket, int channelId, IComputeBuffer recvBuffer) => new BufferedReaderChannel(socket, channelId, recvBuffer);

		/// <summary>
		/// Creates a channel using a socket and receive buffer
		/// </summary>
		/// <param name="socket">Socket to use for sending data</param>
		/// <param name="channelId">Channel id to send and receive data</param>
		public static IComputeChannel CreateChannel(this IComputeSocket socket, int channelId)
		{
			using SharedMemoryBuffer recvBuffer = SharedMemoryBuffer.CreateNew(null, 65536);
			using SharedMemoryBuffer sendBuffer = SharedMemoryBuffer.CreateNew(null, 65536);
			return CreateChannel(socket, channelId, recvBuffer, sendBuffer);
		}

		/// <summary>
		/// Creates a channel using a socket and receive buffer
		/// </summary>
		/// <param name="socket">Socket to use for sending data</param>
		/// <param name="channelId">Channel id to send and receive data</param>
		/// <param name="recvBuffer">Buffer for receiving data</param>
		/// <param name="sendBuffer">Buffer for sending data</param>
		public static IComputeChannel CreateChannel(this IComputeSocket socket, int channelId, IComputeBuffer recvBuffer, IComputeBuffer sendBuffer)
		{
			socket.AttachRecvBuffer(channelId, recvBuffer.Writer);
			socket.AttachSendBuffer(channelId, sendBuffer.Reader);
			return new BufferedReaderWriterChannel(recvBuffer.Reader, sendBuffer.Writer);
		}
	}

	/// <summary>
	/// Extension methods for <see cref="IComputeChannel"/>
	/// </summary>
	public static class ComputeChannelExtensions
	{
		/// <summary>
		/// Reads a complete message from the given socket, retrying reads until the buffer is full.
		/// </summary>
		/// <param name="channel">Channel to read from</param>
		/// <param name="buffer">Buffer to store the data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async ValueTask ReceiveMessageAsync(this IComputeChannel channel, Memory<byte> buffer, CancellationToken cancellationToken = default)
		{
			if (!await TryReceiveMessageAsync(channel, buffer, cancellationToken))
			{
				throw new EndOfStreamException();
			}
		}

		/// <summary>
		/// Reads either a full message or end of stream from the channel
		/// </summary>
		/// <param name="channel">Channel to read from</param>
		/// <param name="buffer">Buffer to store the data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async ValueTask<bool> TryReceiveMessageAsync(this IComputeChannel channel, Memory<byte> buffer, CancellationToken cancellationToken = default)
		{
			for (int offset = 0; offset < buffer.Length;)
			{
				int read = await channel.ReceiveAsync(buffer.Slice(offset), cancellationToken);
				if (read == 0)
				{
					return false;
				}
				offset += read;
			}
			return true;
		}
	}
}
