// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Base interface for input and output buffers
	/// </summary>
	public interface IComputeBuffer : IDisposable
	{
		/// <summary>
		/// Reader for this buffer
		/// </summary>
		IComputeBufferReader Reader { get; }

		/// <summary>
		/// Writer for this buffer
		/// </summary>
		IComputeBufferWriter Writer { get; }

		/// <summary>
		/// Creates a new reference to the underlying buffer. The underlying resources will only be destroyed once all instances are disposed of.
		/// </summary>
		IComputeBuffer AddRef();
	}

	/// <summary>
	/// Read interface for a compute buffer
	/// </summary>
	public interface IComputeBufferReader : IDisposable
	{
		/// <summary>
		/// Create a new reader instance using the same underlying buffer
		/// </summary>
		IComputeBufferReader AddRef();

		/// <summary>
		/// Whether this buffer is complete (no more data will be added)
		/// </summary>
		bool IsComplete { get; }

		/// <summary>
		/// Updates the read position
		/// </summary>
		/// <param name="size">Size of data that was read</param>
		void AdvanceReadPosition(int size);

		/// <summary>
		/// Gets the next data to read
		/// </summary>
		/// <returns>Memory to read from</returns>
		ReadOnlyMemory<byte> GetReadBuffer();

		/// <summary>
		/// Wait for data to be available, or for the buffer to be marked as complete
		/// </summary>
		/// <param name="minLength">Minimum amount of data to read</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>True if new data is available, false if the buffer is complete</returns>
		ValueTask<bool> WaitToReadAsync(int minLength, CancellationToken cancellationToken = default);
	}

	/// <summary>
	/// Buffer that can receive data from a remote machine.
	/// </summary>
	public interface IComputeBufferWriter : IDisposable
	{
		/// <summary>
		/// Create a new writer instance using the same underlying buffer
		/// </summary>
		IComputeBufferWriter AddRef();

		/// <summary>
		/// Mark the output to this buffer as complete
		/// </summary>
		/// <returns>Whether the writer was marked as complete. False if the writer has already been marked as complete.</returns>
		bool MarkComplete();

		/// <summary>
		/// Updates the current write position within the buffer
		/// </summary>
		void AdvanceWritePosition(int size);

		/// <summary>
		/// Gets memory to write to
		/// </summary>
		/// <returns>Memory to be written to</returns>
		Memory<byte> GetWriteBuffer();

		/// <summary>
		/// Gets memory to write to
		/// </summary>
		/// <param name="minLength">Minimum size of the desired write buffer</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Memory to be written to</returns>
		ValueTask WaitToWriteAsync(int minLength, CancellationToken cancellationToken = default);
	}

	/// <summary>
	/// Extension methods for <see cref="IComputeBuffer"/>
	/// </summary>
	public static class ComputeBufferExtensions
	{
		/// <summary>
		/// Read from a buffer into another buffer
		/// </summary>
		/// <param name="reader">Buffer to read from</param>
		/// <param name="buffer">Memory to receive the read data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Number of bytes read</returns>
		public static async ValueTask<int> ReadAsync(this IComputeBufferReader reader, Memory<byte> buffer, CancellationToken cancellationToken = default)
		{
			for (; ; )
			{
				ReadOnlyMemory<byte> readMemory = reader.GetReadBuffer();
				if (reader.IsComplete || readMemory.Length > 0)
				{
					int length = Math.Min(readMemory.Length, buffer.Length);
					readMemory.Slice(0, length).CopyTo(buffer);
					reader.AdvanceReadPosition(length);
					return length;
				}
				await reader.WaitToReadAsync(1, cancellationToken);
			}
		}

		/// <summary>
		/// Writes data into a buffer from a memory block
		/// </summary>
		/// <param name="writer">Writer to output the data to</param>
		/// <param name="buffer">The data to write</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async ValueTask WriteAsync(this IComputeBufferWriter writer, ReadOnlyMemory<byte> buffer, CancellationToken cancellationToken = default)
		{
			while (buffer.Length > 0)
			{
				Memory<byte> writeMemory = writer.GetWriteBuffer();
				if (writeMemory.Length >= buffer.Length)
				{
					buffer.CopyTo(writeMemory);
					writer.AdvanceWritePosition(buffer.Length);
					break;
				}
				await writer.WaitToWriteAsync(writeMemory.Length, cancellationToken);
			}
		}
	}
}
