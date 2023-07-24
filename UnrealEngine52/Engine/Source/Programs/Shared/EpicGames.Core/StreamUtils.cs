// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Core
{
	/// <summary>
	/// Utility methods for streams
	/// </summary>
	public static class StreamUtils
	{
		/// <summary>
		/// Issues read calls until the given buffer has been filled with data or the stream is at an end
		/// </summary>
		/// <param name="stream">Stream to read from</param>
		/// <param name="buffer">Buffer to receive the output data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async Task<int> ReadGreedyAsync(this Stream stream, Memory<byte> buffer, CancellationToken cancellationToken = default)
		{
			int length = 0;
			while (length < buffer.Length)
			{
				int readLength = await stream.ReadAsync(buffer.Slice(length), cancellationToken);
				if (readLength == 0)
				{
					break;
				}
				length += readLength;
			}
			return length;
		}

		/// <summary>
		/// Reads a fixed amount of data from a stream, throwing an exception if the entire buffer cannot be read.
		/// </summary>
		/// <param name="stream">Stream to read from</param>
		/// <param name="buffer">Buffer to receive the output data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async Task ReadFixedLengthBytesAsync(this Stream stream, Memory<byte> buffer, CancellationToken cancellationToken = default)
		{
			for (int offset = 0; offset < buffer.Length;)
			{
				int readLength = await stream.ReadAsync(buffer.Slice(offset), cancellationToken);
				if (readLength == 0)
				{
					throw new EndOfStreamException($"Unexpected end of stream while trying to read {buffer.Length} bytes.");
				}
				offset += readLength;
			}
		}
	}
}
