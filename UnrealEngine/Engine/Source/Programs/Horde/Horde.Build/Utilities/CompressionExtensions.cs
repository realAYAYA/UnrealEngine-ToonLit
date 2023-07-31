// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers.Binary;
using System.Globalization;
using System.IO;
using EpicGames.Core;
using ICSharpCode.SharpZipLib.BZip2;
using OpenTracing;
using OpenTracing.Util;

namespace Horde.Build.Utilities
{
	/// <summary>
	/// Extension methods for compression
	/// </summary>
	static class CompressionExtensions
	{
		/// <summary>
		/// Compress a block of data with bzip2
		/// </summary>
		/// <returns>The compressed data</returns>
		public static byte[] CompressBzip2(this ReadOnlyMemory<byte> memory)
		{
			using IScope scope = GlobalTracer.Instance.BuildSpan("CompressBzip2").StartActive();
			scope.Span.SetTag("DecompressedSize", memory.Length.ToString(CultureInfo.InvariantCulture));

			byte[] compressedData;
			using (MemoryStream stream = new MemoryStream())
			{
				byte[] decompressedSize = new byte[4];
				BinaryPrimitives.WriteInt32LittleEndian(decompressedSize.AsSpan(), memory.Length);
				stream.Write(decompressedSize.AsSpan());

				using (BZip2OutputStream compressedStream = new BZip2OutputStream(stream))
				{
					compressedStream.Write(memory.Span);
				}

				compressedData = stream.ToArray();
			}

			scope.Span.SetTag("CompressedSize", compressedData.Length.ToString(CultureInfo.InvariantCulture));
			return compressedData;
		}

		/// <summary>
		/// Decompress the data
		/// </summary>
		/// <returns>The decompressed data</returns>
		public static byte[] DecompressBzip2(this ReadOnlyMemory<byte> memory)
		{
			int decompressedSize = BinaryPrimitives.ReadInt32LittleEndian(memory.Span);

			using IScope scope = GlobalTracer.Instance.BuildSpan("DecompressBzip2").StartActive();
			scope.Span.SetTag("CompressedSize", memory.Length.ToString(CultureInfo.InvariantCulture));
			scope.Span.SetTag("DecompressedSize", decompressedSize.ToString(CultureInfo.InvariantCulture));

			byte[] data = new byte[decompressedSize];
			using (ReadOnlyMemoryStream stream = new ReadOnlyMemoryStream(memory.Slice(4)))
			{
				using (BZip2InputStream decompressedStream = new BZip2InputStream(stream))
				{
					int readSize = decompressedStream.Read(data.AsSpan());
					if (readSize != data.Length)
					{
						throw new InvalidDataException($"Compressed data is too short (expected {data.Length} bytes, got {readSize} bytes)");
					}
				}
			}
			return data;
		}
	}
}
