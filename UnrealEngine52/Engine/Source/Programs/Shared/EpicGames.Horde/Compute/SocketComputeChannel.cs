// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Serialization;
using System;
using System.IO;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;

#pragma warning disable CA5401

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Transfers messages via a AES-encrypted socket
	/// </summary>
	public class SocketComputeChannel : IComputeChannel
	{
		readonly Socket _socket;
		readonly int _blockSize;
		readonly ICryptoTransform _decryptor;
		readonly ICryptoTransform _encryptor;

		readonly CbWriter _writer;

		byte[] _readBuffer = new byte[128];
		int _decryptedLength;
		int _encryptedLength;
		int _paddedMessageLength;

		byte[] _writeBuffer = new byte[128];

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="socket">Socket for communication</param>
		/// <param name="aesKey">AES encryption key</param>
		/// <param name="aesIv">AES initialization vector</param>
		public SocketComputeChannel(Socket socket, ReadOnlyMemory<byte> aesKey, ReadOnlyMemory<byte> aesIv)
		{
			using Aes aes = Aes.Create();
			aes.Key = aesKey.ToArray();
			aes.IV = aesIv.ToArray();
			aes.Padding = PaddingMode.None;

			_socket = socket;
			_blockSize = aes.BlockSize / 8;
			_decryptor = aes.CreateDecryptor();
			_encryptor = aes.CreateEncryptor();

			_writer = new CbWriter();
		}

		/// <summary>
		/// Receives a message from the remote
		/// </summary>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		public async Task<object> ReadAsync(CancellationToken cancellationToken)
		{
			// Discard any data from the previous message
			if (_paddedMessageLength > 0)
			{
				Buffer.BlockCopy(_readBuffer, _paddedMessageLength, _readBuffer, 0, _encryptedLength - _paddedMessageLength);
				_encryptedLength -= _paddedMessageLength;
				_decryptedLength -= _paddedMessageLength;
			}

			// Read the next message data
			for (; ; )
			{
				// Check the object in the buffer
				if (_decryptedLength >= 2)
				{
					int messageLength = 1 + VarInt.Measure(_readBuffer[1]);
					if (_decryptedLength > messageLength)
					{
						messageLength += (int)VarInt.ReadUnsigned(_readBuffer.AsSpan(1));
					}

					_paddedMessageLength = messageLength + GetPadding(messageLength);

					if (_decryptedLength >= _paddedMessageLength)
					{
						return ComputeMessage.Deserialize(_readBuffer.AsMemory(0, messageLength));
					}

					if (_readBuffer.Length < _paddedMessageLength)
					{
						Array.Resize(ref _readBuffer, _paddedMessageLength);
					}
				}

				// Read the next chunk of data from the socket
				int read = await _socket.ReceiveAsync(_readBuffer.AsMemory(_encryptedLength), SocketFlags.Partial, cancellationToken);
				if (read == 0)
				{
					throw new EndOfStreamException();
				}
				_encryptedLength += read;

				// Decrypt any new full blocks that have been received
				int nextTransformSize = _encryptedLength - _decryptedLength;
				if (nextTransformSize >= _blockSize)
				{
					nextTransformSize -= nextTransformSize % _blockSize;
					_decryptedLength += _decryptor.TransformBlock(_readBuffer, _decryptedLength, nextTransformSize, _readBuffer, _decryptedLength);
				}
			}
		}

		/// <summary>
		/// Sends a message to the remote
		/// </summary>
		/// <param name="message">Message to be sent</param>
		/// <param name="cancellationToken">Cancellation token</param>
		/// <returns></returns>
		public async Task WriteAsync(object message, CancellationToken cancellationToken)
		{
			// Format the output object
			ComputeMessage.Serialize(message, _writer);

			// Serialize it to the write buffer
			int length = _writer.GetSize();
			length += GetPadding(length);

			if (length > _writeBuffer.Length)
			{
				_writeBuffer = new byte[length];
			}

			_writer.CopyTo(_writeBuffer);

			// Encrypt the data and send it
			int transformed = _encryptor.TransformBlock(_writeBuffer, 0, length, _writeBuffer, 0);
			if (transformed != length)
			{
				throw new InvalidOperationException();
			}
			await _socket.SendAsync(_writeBuffer.AsMemory(0, length), SocketFlags.Partial, cancellationToken);
		}

		int GetPadding(int length)
		{
			int modulo = length % _blockSize;
			if (modulo == 0)
			{
				return 0;
			}
			else
			{
				return _blockSize - modulo;
			}
		}
	}
}
