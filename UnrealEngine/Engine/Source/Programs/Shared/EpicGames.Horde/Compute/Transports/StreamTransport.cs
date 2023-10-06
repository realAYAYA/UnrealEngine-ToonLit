// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Compute.Transports
{
	/// <summary>
	/// Compute transport which wraps an underlying stream
	/// </summary>
	class StreamTransport : IComputeTransport
	{
		readonly Stream _stream;

		/// <inheritdoc/>
		public long Position { get; private set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="stream">Stream to use for the transferring data</param>
		public StreamTransport(Stream stream) => _stream = stream;

		/// <inheritdoc/>
		public async ValueTask<int> ReadPartialAsync(Memory<byte> buffer, CancellationToken cancellationToken)
		{
			int length = await _stream.ReadAsync(buffer, cancellationToken);
			Position += length;
			return length;
		}

		/// <inheritdoc/>
		public async ValueTask WriteAsync(ReadOnlySequence<byte> buffer, CancellationToken cancellationToken)
		{
			foreach (ReadOnlyMemory<byte> memory in buffer)
			{
				await _stream.WriteAsync(memory, cancellationToken);
				Position += memory.Length;
			}
		}

		/// <inheritdoc/>
		public ValueTask MarkCompleteAsync(CancellationToken cancellationToken) => new ValueTask();
	}
}
