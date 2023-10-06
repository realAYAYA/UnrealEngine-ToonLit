// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Buffers.Binary;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Compute.Buffers;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Implementation of a compute channel
	/// </summary>
	public sealed class AgentMessageChannel : IDisposable
	{
		// Length of a message header. Consists of a 1 byte type field, followed by 4 byte length field.
		const int HeaderLength = 5;

		/// <summary>
		/// Allows creating new messages in rented memory
		/// </summary>
		class MessageBuilder : IAgentMessageBuilder
		{
			readonly AgentMessageChannel _channel;
			readonly IComputeBufferWriter _sendBufferWriter;
			readonly AgentMessageType _type;
			int _length;

			/// <inheritdoc/>
			public int Length => _length;

			public MessageBuilder(AgentMessageChannel channel, IComputeBufferWriter sendBufferWriter, AgentMessageType type)
			{
				_channel = channel;
				_sendBufferWriter = sendBufferWriter;
				_type = type;
				_length = 0;
			}

			public void Dispose()
			{
				if (_channel._currentBuilder == this)
				{
					_channel._currentBuilder = null;
				}
			}

			public void Send()
			{
				Span<byte> header = _sendBufferWriter.GetWriteBuffer().Span;
				header[0] = (byte)_type;
				BinaryPrimitives.WriteInt32LittleEndian(header.Slice(1, 4), _length);

				if (_channel._logger.IsEnabled(LogLevel.Trace))
				{
					_channel.LogMessageInfo("SEND", _type, _sendBufferWriter.GetWriteBuffer().Slice(HeaderLength, _length).Span);
				}

				_sendBufferWriter.AdvanceWritePosition(HeaderLength + _length);
				_length = 0;
			}

			/// <inheritdoc/>
			public void Advance(int count) => _length += count;

			/// <inheritdoc/>
			public Memory<byte> GetMemory(int sizeHint = 0) => _sendBufferWriter.GetWriteBuffer().Slice(HeaderLength + _length);

			/// <inheritdoc/>
			public Span<byte> GetSpan(int sizeHint = 0) => GetMemory(sizeHint).Span;
		}

		readonly IComputeSocket _socket;
		readonly IComputeBufferReader _recvBufferReader;
		readonly IComputeBufferWriter _sendBufferWriter;

		// Can lock chunked memory writer to acuqire pointer
		readonly ILogger _logger;

#pragma warning disable CA2213
		MessageBuilder? _currentBuilder;
#pragma warning restore CA2213

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="recvBufferReader"></param>
		/// <param name="sendBufferWriter"></param>
		/// <param name="logger">Logger for diagnostic output</param>
		public AgentMessageChannel(IComputeBufferReader recvBufferReader, IComputeBufferWriter sendBufferWriter, ILogger logger)
		{
			_socket = null!;
			_recvBufferReader = recvBufferReader.AddRef();
			_sendBufferWriter = sendBufferWriter.AddRef();
			_logger = logger;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="socket"></param>
		/// <param name="channelId"></param>
		/// <param name="recvBuffer"></param>
		/// <param name="sendBuffer"></param>
		/// <param name="logger">Logger for diagnostic output</param>
		public AgentMessageChannel(IComputeSocket socket, int channelId, IComputeBuffer recvBuffer, IComputeBuffer sendBuffer, ILogger logger)
		{
			_socket = socket;
			_socket.AttachRecvBuffer(channelId, recvBuffer.Writer);
			_socket.AttachSendBuffer(channelId, sendBuffer.Reader);
			_recvBufferReader = recvBuffer.Reader.AddRef();
			_sendBufferWriter = sendBuffer.Writer.AddRef();
			_logger = logger;
		}

		/// <summary>
		/// Overridable dispose method
		/// </summary>
		public void Dispose()
		{
			_currentBuilder?.Dispose();

			_sendBufferWriter.MarkComplete();
			_sendBufferWriter.Dispose();

			_recvBufferReader.Dispose();
		}

		/// <summary>
		/// Mark the send buffer as complete
		/// </summary>
		public void MarkComplete()
		{
			_sendBufferWriter.MarkComplete();
		}

		/// <inheritdoc/>
		public async ValueTask<AgentMessage> ReceiveAsync(CancellationToken cancellationToken)
		{
			while (!_recvBufferReader.IsComplete)
			{
				ReadOnlyMemory<byte> memory = _recvBufferReader.GetReadBuffer();
				if (memory.Length < HeaderLength)
				{
					await _recvBufferReader.WaitToReadAsync(HeaderLength, cancellationToken);
					continue;
				}

				int messageLength = BinaryPrimitives.ReadInt32LittleEndian(memory.Span.Slice(1, 4));
				if (memory.Length < HeaderLength + messageLength)
				{
					await _recvBufferReader.WaitToReadAsync(HeaderLength + messageLength, cancellationToken);
					continue;
				}

				AgentMessageType type = (AgentMessageType)memory.Span[0];
				AgentMessage message = new AgentMessage(type, memory.Slice(HeaderLength, messageLength));
				if (_logger.IsEnabled(LogLevel.Trace))
				{
					LogMessageInfo("RECV", message.Type, message.Data.Span);
				}

				_recvBufferReader.AdvanceReadPosition(HeaderLength + messageLength);
				return message;
			}
			return new AgentMessage(AgentMessageType.None, ReadOnlyMemory<byte>.Empty);
		}

		void LogMessageInfo(string verb, AgentMessageType type, ReadOnlySpan<byte> data)
		{
			StringBuilder bytes = new StringBuilder();
			for (int offset = 0; offset < 16 && offset < data.Length; offset++)
			{
				bytes.Append($" {data[offset]:X2}");
			}
			if (data.Length > 16)
			{
				bytes.Append("..");
			}
			_logger.LogTrace("{Verb} {Type,-22} [{Length,10:n0}] = {Bytes}", verb, type, data.Length, bytes.ToString());
		}

		/// <inheritdoc/>
		public async ValueTask<IAgentMessageBuilder> CreateMessageAsync(AgentMessageType type, int maxSize, CancellationToken cancellationToken)
		{
			if (_currentBuilder != null)
			{
				throw new InvalidOperationException("Only one writer can be active at a time. Dispose of the previous writer first.");
			}

			await _sendBufferWriter.WaitToWriteAsync(maxSize, cancellationToken);

			_currentBuilder = new MessageBuilder(this, _sendBufferWriter, type);
			return _currentBuilder;
		}
	}

	/// <summary>
	/// Extension methods to allow creating channels from leases
	/// </summary>
	public static class AgentMessageChannelExtensions
	{
		/// <summary>
		/// Creates a message channel with the given identifier
		/// </summary>
		/// <param name="socket">Socket to create a channel for</param>
		/// <param name="channelId">Identifier for the channel</param>
		/// <param name="logger">Logger for the channel</param>
		public static AgentMessageChannel CreateAgentMessageChannel(this IComputeSocket socket, int channelId, ILogger logger)
			=> socket.CreateAgentMessageChannel(channelId, 65536, logger);

		/// <summary>
		/// Creates a message channel with the given identifier
		/// </summary>
		/// <param name="socket">Socket to create a channel for</param>
		/// <param name="channelId">Identifier for the channel</param>
		/// <param name="bufferSize">Size of the send and receive buffer</param>
		/// <param name="logger">Logger for the channel</param>
		public static AgentMessageChannel CreateAgentMessageChannel(this IComputeSocket socket, int channelId, int bufferSize, ILogger logger)
			=> socket.CreateAgentMessageChannel(channelId, bufferSize, bufferSize, logger);

		/// <summary>
		/// Creates a message channel with the given identifier
		/// </summary>
		/// <param name="socket">Socket to create a channel for</param>
		/// <param name="channelId">Identifier for the channel</param>
		/// <param name="sendBufferSize">Size of the send buffer</param>
		/// <param name="recvBufferSize">Size of the recieve buffer</param>
		/// <param name="logger">Logger for the channel</param>
		public static AgentMessageChannel CreateAgentMessageChannel(this IComputeSocket socket, int channelId, int sendBufferSize, int recvBufferSize, ILogger logger)
		{
			using IComputeBuffer sendBuffer = new PooledBuffer(sendBufferSize);
			using IComputeBuffer recvBuffer = new PooledBuffer(recvBufferSize);
			return new AgentMessageChannel(socket, channelId, sendBuffer, recvBuffer, logger);
		}

		/// <summary>
		/// Reads a message from the channel
		/// </summary>
		/// <param name="channel">Channel to receive on</param>
		/// <param name="type">Expected type of the message</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Data for a message that was read. Must be disposed.</returns>
		public static async ValueTask<AgentMessage> ReceiveAsync(this AgentMessageChannel channel, AgentMessageType type, CancellationToken cancellationToken = default)
		{
			AgentMessage message = await channel.ReceiveAsync(cancellationToken);
			if (message.Type != type)
			{
				throw new InvalidAgentMessageException(message);
			}
			return message;
		}

		/// <summary>
		/// Creates a new builder for a message
		/// </summary>
		/// <param name="channel">Channel to send on</param>
		/// <param name="type">Type of the message</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>New builder for messages</returns>
		public static ValueTask<IAgentMessageBuilder> CreateMessageAsync(this AgentMessageChannel channel, AgentMessageType type, CancellationToken cancellationToken)
		{
			return channel.CreateMessageAsync(type, 1024, cancellationToken);
		}

		/// <summary>
		/// Forwards an existing message across a channel
		/// </summary>
		/// <param name="channel">Channel to send on</param>
		/// <param name="message">The message to be sent</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async ValueTask SendAsync(this AgentMessageChannel channel, AgentMessage message, CancellationToken cancellationToken)
		{
			using (IAgentMessageBuilder builder = await channel.CreateMessageAsync(message.Type, message.Data.Length, cancellationToken))
			{
				builder.WriteFixedLengthBytes(message.Data.Span);
				builder.Send();
			}
		}
	}
}
