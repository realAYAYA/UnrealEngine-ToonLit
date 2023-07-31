// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Threading.Tasks;

namespace Horde.Build.Utilities
{
	/// <summary>
	/// Extension methods for working with streams
	/// </summary>
	static class StreamExtensions
	{
		/// <summary>
		/// Reads data of a fixed size into the buffer. Throws an exception if the whole block cannot be read.
		/// </summary>
		/// <param name="stream">The stream to read from</param>
		/// <param name="data">Buffer to receive the read data</param>
		/// <param name="offset">Offset within the buffer to read the new data</param>
		/// <param name="length">Length of the data to read</param>
		/// <returns>Async task</returns>
		public static Task ReadFixedSizeDataAsync(this Stream stream, byte[] data, int offset, int length)
		{
			return ReadFixedSizeDataAsync(stream, data.AsMemory(offset, length));
		}

		/// <summary>
		/// Reads data of a fixed size into the buffer. Throws an exception if the whole block cannot be read.
		/// </summary>
		/// <param name="stream">The stream to read from</param>
		/// <param name="data">Buffer to receive the read data</param>
		/// <returns>Async task</returns>
		public static async Task ReadFixedSizeDataAsync(this Stream stream, Memory<byte> data)
		{
			while (data.Length > 0)
			{
				int count = await stream.ReadAsync(data);
				if (count == 0)
				{
					throw new EndOfStreamException();
				}
				data = data.Slice(count);
			}
		}
	}
}