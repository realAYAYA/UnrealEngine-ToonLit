// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Buffers.Binary;
using System.IO.Pipelines;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Compute.Transports
{
	/// <summary>
	/// Transport layer that adds AES encryption on top of an underlying transport implementation. Key must be exchanged separately
	/// (eg. via the HTTPS request to negotiate a lease with the server).
	/// </summary>
	public sealed class AesTransport : IComputeTransport, IAsyncDisposable
	{
		/// <summary>
		/// Length of the required encrption key. 
		/// </summary>
		public const int KeyLength = 32;

		/// <summary>
		/// Length of the nonce. This should be a cryptographically random number, and does not have to be secret.
		/// </summary>
		public const int NonceLength = 12;

		const int HeaderLength = sizeof(int); // Unencrypted size of the message
		const int FooterLength = 16; // 16-byte auth tag for encryption

		readonly IComputeTransport _inner;
		readonly Pipe _readPipe;
		readonly AesGcm _aesGcm;

		ReadOnlySequence<byte> _readBuffer;

		readonly byte[] _readNonce;
		readonly byte[] _writeNonce;

		readonly IMemoryOwner<byte> _writeBufferOwner;
		Task _lastWriteTask = Task.CompletedTask;
		int _writeBufferOffset = 0;

		const int WritePacketSize = 64 * 1024;
		readonly BackgroundTask _backgroundReadTask;

		/// <inheritdoc/>
		public long Position { get; private set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="inner">The underlying transport implementation</param>
		/// <param name="key">AES encryption key (256 bits / 32 bytes)</param>
		/// <param name="nonce">Cryptographic nonce to identify the connection. Must be longer than <see cref="NonceLength"/>.</param>
		public AesTransport(IComputeTransport inner, ReadOnlySpan<byte> key, ReadOnlySpan<byte> nonce)
		{
			if (key.Length != KeyLength)
			{
				throw new ArgumentException($"Key must be {KeyLength} bytes", nameof(key));
			}
			if (nonce.Length < NonceLength)
			{
				throw new ArgumentException($"Nonce must be at least {NonceLength} bytes", nameof(nonce));
			}

			_inner = inner;
			_readPipe = new Pipe();
			_aesGcm = new AesGcm(key);
			_readNonce = nonce.Slice(0, NonceLength).ToArray();
			_writeNonce = nonce.Slice(0, NonceLength).ToArray();

			_backgroundReadTask = BackgroundTask.StartNew(BackgroundReadAsync);

			_writeBufferOwner = MemoryPool<byte>.Shared.Rent(WritePacketSize * 2);
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			await _lastWriteTask;
			await _backgroundReadTask.DisposeAsync();
			_writeBufferOwner.Dispose();
			_aesGcm.Dispose();
		}

		/// <summary>
		/// Creates an encryption key
		/// </summary>
		public static byte[] CreateKey() => RandomNumberGenerator.GetBytes(KeyLength);

		async Task BackgroundReadAsync(CancellationToken cancellationToken)
		{
			using IMemoryOwner<byte> buffer = MemoryPool<byte>.Shared.Rent(128 * 1024);
			Memory<byte> memory = buffer.Memory;

			int size = 0;
			for (; ; )
			{
				// Read more data into the buffer
				int read = await _inner.ReadPartialAsync(memory.Slice(size), cancellationToken);
				if (read == 0)
				{
					break;
				}
				size += read;

				// Wait until we've got a complete encryption packet
				int prevSize = size;
				size = Decrypt(memory.Slice(0, size).Span);
				memory.Slice(prevSize - size, size).CopyTo(memory);
			}

			await _readPipe.Writer.CompleteAsync();
		}

		int Decrypt(Span<byte> span)
		{
			while (span.Length >= HeaderLength)
			{
				ReadOnlySpan<byte> header = span.Slice(0, HeaderLength);

				int length = BinaryPrimitives.ReadInt32LittleEndian(header);
				if (span.Length < HeaderLength + length + FooterLength)
				{
					break;
				}

				ReadOnlySpan<byte> cipherText = span.Slice(HeaderLength, length);
				ReadOnlySpan<byte> tag = span.Slice(HeaderLength + length, FooterLength);
				Span<byte> plainText = _readPipe.Writer.GetSpan(length).Slice(0, length);

				_aesGcm.Decrypt(_readNonce, cipherText, tag, plainText, header);
				IncrementNonce(_readNonce);

				_readPipe.Writer.Advance(length);

				span = span.Slice(HeaderLength + length + FooterLength);
			}
			return span.Length;
		}

		/// <inheritdoc/>
		public async ValueTask<int> ReadPartialAsync(Memory<byte> buffer, CancellationToken cancellationToken)
		{
			int sizeRead = 0;
			while (sizeRead == 0)
			{
				// Try to get more data into the read buffer
				if (_readBuffer.Length == 0)
				{
					ReadResult result = await _readPipe.Reader.ReadAsync(cancellationToken);
					_readBuffer = result.Buffer;
				}

				// Copy as much of the next chunk as we can
				int copySize = Math.Min(buffer.Length, _readBuffer.First.Length);
				_readBuffer.First.Slice(0, copySize).CopyTo(buffer);
				buffer = buffer.Slice(copySize);
				_readBuffer = _readBuffer.Slice(copySize);
				sizeRead += copySize;
				Position += copySize;
			}
			return sizeRead;
		}

		/// <inheritdoc/>
		public async ValueTask WriteAsync(ReadOnlySequence<byte> buffer, CancellationToken cancellationToken)
		{
			while (buffer.Length > 0)
			{
				ReadOnlyMemory<byte> plainText = buffer.First;

				int maxSize = WritePacketSize - HeaderLength - FooterLength;
				if (plainText.Length > maxSize)
				{
					plainText = plainText.Slice(0, maxSize);
				}

				Memory<byte> writeBuffer = _writeBufferOwner.Memory.Slice(_writeBufferOffset, HeaderLength + plainText.Length + FooterLength);
				Encrypt(plainText.Span, writeBuffer.Span);

				Position += plainText.Length;

				await _lastWriteTask;
				_lastWriteTask = _inner.WriteAsync(writeBuffer, cancellationToken).AsTask();

				buffer = buffer.Slice(plainText.Length);
				_writeBufferOffset ^= WritePacketSize;
			}
		}

		void Encrypt(ReadOnlySpan<byte> input, Span<byte> output)
		{
			int length = input.Length;

			Span<byte> header = output.Slice(0, HeaderLength);
			BinaryPrimitives.WriteInt32LittleEndian(header, input.Length);

			Span<byte> cipherText = output.Slice(HeaderLength, input.Length);
			Span<byte> tag = output.Slice(HeaderLength + length, FooterLength);

			_aesGcm.Encrypt(_writeNonce, input, cipherText, tag, header);
			IncrementNonce(_writeNonce);
		}

		static void IncrementNonce(byte[] nonce)
		{
			BinaryPrimitives.WriteInt64LittleEndian(nonce, BinaryPrimitives.ReadInt64LittleEndian(nonce) + 1);
		}

		/// <inheritdoc/>
		public ValueTask MarkCompleteAsync(CancellationToken cancellationToken) => _inner.MarkCompleteAsync(cancellationToken);
	}
}
