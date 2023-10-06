// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Low-level interface for transferring data
	/// </summary>
	public interface IComputeTransport
	{
		/// <summary>
		/// Position in the stream; used for debugging
		/// </summary>
		long Position { get; }

		/// <summary>
		/// Reads data from the underlying transport into an output buffer
		/// </summary>
		/// <param name="buffer">Buffer to read into</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		ValueTask<int> ReadPartialAsync(Memory<byte> buffer, CancellationToken cancellationToken);

		/// <summary>
		/// Writes data to the underlying transport
		/// </summary>
		/// <param name="buffer">Buffer to be written</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		ValueTask WriteAsync(ReadOnlySequence<byte> buffer, CancellationToken cancellationToken);

		/// <summary>
		/// Indicate that all data has been written to the transport layer, and that there will be no more calls to <see cref="WriteAsync(ReadOnlySequence{Byte}, CancellationToken)"/>
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		ValueTask MarkCompleteAsync(CancellationToken cancellationToken);
	}

	/// <summary>
	/// Extension methods for <see cref="IComputeTransport"/>
	/// </summary>
	public static class ComputeTransportExtensions
	{
		/// <summary>
		/// Fill the given buffer with data
		/// </summary>
		/// <param name="transport">Transport object</param>
		/// <param name="buffer">Buffer to read into</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async ValueTask ReadFullAsync(this IComputeTransport transport, Memory<byte> buffer, CancellationToken cancellationToken)
		{
			int read = 0;
			while (read < buffer.Length)
			{
				int partialRead = await transport.ReadPartialAsync(buffer.Slice(read, buffer.Length - read), cancellationToken);
				if (partialRead == 0)
				{
					throw new EndOfStreamException();
				}
				read += partialRead;
			}
		}

		/// <summary>
		/// Fill the given buffer with data
		/// </summary>
		/// <param name="transport">Transport object</param>
		/// <param name="buffer">Buffer to read into</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async ValueTask<bool> ReadOptionalAsync(this IComputeTransport transport, Memory<byte> buffer, CancellationToken cancellationToken)
		{
			int read = await transport.ReadPartialAsync(buffer, cancellationToken);
			if (read == 0)
			{
				return false;
			}
			if (read < buffer.Length)
			{
				await transport.ReadFullAsync(buffer.Slice(read), cancellationToken);
			}
			return true;
		}

		/// <summary>
		/// Writes data to the underlying transport
		/// </summary>
		/// <param name="transport">Transport instance</param>
		/// <param name="buffer">Buffer to be written</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static ValueTask WriteAsync(this IComputeTransport transport, ReadOnlyMemory<byte> buffer, CancellationToken cancellationToken)
			=> transport.WriteAsync(new ReadOnlySequence<byte>(buffer), cancellationToken);
	}
}

