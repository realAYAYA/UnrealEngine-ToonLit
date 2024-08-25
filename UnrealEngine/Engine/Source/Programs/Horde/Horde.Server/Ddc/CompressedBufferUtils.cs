// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Blake3;
using EpicGames.Compression;
using EpicGames.Core;
using Force.Crc32;
using K4os.Compression.LZ4;
using OpenTelemetry.Trace;

#pragma warning disable CS1591 // Missing XML comment for public type

namespace Horde.Server.Ddc
{
	public class CompressedBufferHeader
	{
		[System.Diagnostics.CodeAnalysis.SuppressMessage("Design", "CA1028:Enum Storage should be Int32", Justification = "Interop requires byte")]
		public enum CompressionMethod : byte
		{
			// Header is followed by one uncompressed block. 
			None = 0,
			// Header is followed by an array of compressed block sizes then the compressed blocks. 
			Oodle = 3,
			LZ4 = 4,
		}

		public const uint ExpectedMagic = 0xb7756362; // <dot>ucb
		public const uint HeaderLength = 64;

		// A magic number to identify a compressed buffer. Always 0xb7756362.

		public uint Magic { get; set; }
		// A CRC-32 used to check integrity of the buffer. Uses the polynomial 0x04c11db7.

		public uint Crc32 { get; set; }

		// The method used to compress the buffer. Affects layout of data following the header. 
		public CompressionMethod Method { get; set; }
		public byte CompressionLevel { get; set; }
		public byte CompressionMethodUsed { get; set; }

		// The power of two size of every uncompressed block except the last. Size is 1 << BlockSizeExponent. 
		public byte BlockSizeExponent { get; set; }

		// The number of blocks that follow the header. 
		public uint BlockCount { get; set; }

		// The total size of the uncompressed data. 
		public ulong TotalRawSize { get; set; }

		// The total size of the compressed data including the header. 
		public ulong TotalCompressedSize { get; set; }

		/** The hash of the uncompressed data. */
		public byte[] RawHash { get; set; } = Array.Empty<byte>();

		public void ByteSwap()
		{
			Magic = BinaryPrimitives.ReverseEndianness(Magic);
			Crc32 = BinaryPrimitives.ReverseEndianness(Crc32);
			BlockCount = BinaryPrimitives.ReverseEndianness(BlockCount);
			TotalRawSize = BinaryPrimitives.ReverseEndianness(TotalRawSize);
			TotalCompressedSize = BinaryPrimitives.ReverseEndianness(TotalCompressedSize);
		}
	}

	public class CompressedBufferUtils
	{
		private readonly Tracer _tracer;
		private readonly BufferedPayloadFactory _payloadFactory;

		public CompressedBufferUtils(Tracer tracer, BufferedPayloadFactory payloadFactory)
		{
			_tracer = tracer;
			_payloadFactory = payloadFactory;
		}

		private static (CompressedBufferHeader, uint[]) ExtractHeader(BinaryReader br)
		{
			byte[] headerData = br.ReadBytes((int)CompressedBufferHeader.HeaderLength);

			using MemoryStream ms = new MemoryStream(headerData);
			using BinaryReader reader = new BinaryReader(ms);

			// the header is always stored big endian
			bool needsByteSwap = BitConverter.IsLittleEndian;

			CompressedBufferHeader header = new CompressedBufferHeader
			{
				Magic = reader.ReadUInt32(),
				Crc32 = reader.ReadUInt32(),
				Method = (CompressedBufferHeader.CompressionMethod)reader.ReadByte(),
				CompressionLevel = reader.ReadByte(),
				CompressionMethodUsed = reader.ReadByte(),
				BlockSizeExponent = reader.ReadByte(),
				BlockCount = reader.ReadUInt32(),
				TotalRawSize = reader.ReadUInt64(),
				TotalCompressedSize = reader.ReadUInt64()
			};
			byte[] hash = reader.ReadBytes(32); // a full blake3 hash
			header.RawHash = hash;

			if (needsByteSwap)
			{
				header.ByteSwap();
			}

			if (header.Magic != CompressedBufferHeader.ExpectedMagic)
			{
				throw new InvalidMagicException(header.Magic, CompressedBufferHeader.ExpectedMagic);
			}

			// calculate the crc from the start of the method field (skipping magic which is a constant and the crc field itself)
			const int MethodOffset = sizeof(uint) + sizeof(uint);

			// none compressed objects have no extra blocks
			uint blocksByteUsed = header.Method != CompressedBufferHeader.CompressionMethod.None ? header.BlockCount * (uint)sizeof(uint) : 0;

			byte[] crcData = new byte[blocksByteUsed + headerData.Length];
			Array.Copy(headerData, crcData, headerData.Length);

			uint[] blocks = Array.Empty<uint>();

			if (blocksByteUsed != 0)
			{
				byte[] blocksData = br.ReadBytes((int)blocksByteUsed);

				Array.Copy(blocksData, 0, crcData, headerData.Length, blocksData.Length);

				blocks = new uint[header.BlockCount];

				for (int i = 0; i < header.BlockCount; i++)
				{
					ReadOnlySpan<byte> memory = new ReadOnlySpan<byte>(blocksData, i * sizeof(uint), sizeof(uint));
					uint compressedBlockSize = BinaryPrimitives.ReadUInt32BigEndian(memory);
					blocks[i] = compressedBlockSize;
				}
			}

			uint calculatedCrc = Crc32Algorithm.Compute(crcData, MethodOffset, (int)(CompressedBufferHeader.HeaderLength - MethodOffset + blocksByteUsed));

			if (header.Crc32 != calculatedCrc)
			{
				throw new InvalidHashException(header.Crc32, calculatedCrc);
			}

			return (header, blocks);
		}

		public static void WriteHeader(CompressedBufferHeader header, BinaryWriter writer)
		{
			// the header is always stored big endian
			bool needsByteSwap = BitConverter.IsLittleEndian;
			if (needsByteSwap)
			{
				header.ByteSwap();
			}

			writer.Write(header.Magic);
			writer.Write(header.Crc32);
			writer.Write((byte)header.Method);
			writer.Write((byte)header.CompressionLevel);
			writer.Write((byte)header.CompressionMethodUsed);
			writer.Write((byte)header.BlockSizeExponent);
			writer.Write(header.BlockCount);
			writer.Write(header.TotalRawSize);
			writer.Write(header.TotalCompressedSize);
			writer.Write(header.RawHash, 0, 20); // write the first 20 bytes as iohashes are 20 bytes
			for (int i = 0; i < 12; i++)
			{
				// the last 12 bytes should be 0 as they are reserved
				writer.Write((byte)0);
			}

			if (needsByteSwap)
			{
				header.ByteSwap();
			}
		}

		public async Task<IBufferedPayload> DecompressContentAsync(Stream sourceStream, ulong streamSize, CancellationToken cancellationToken = default)
		{
			using BinaryReader br = new BinaryReader(sourceStream);
			(CompressedBufferHeader header, uint[] compressedBlockSizes) = ExtractHeader(br);

			if (streamSize < header.TotalCompressedSize)
			{
				throw new Exception($"Expected stream to be {header.TotalCompressedSize} but it was {streamSize}");
			}

			using FilesystemBufferedPayloadWriter bufferedPayloadWriter = _payloadFactory.CreateFilesystemBufferedPayloadWriter();

			{
				await using Stream targetStream = bufferedPayloadWriter.GetWritableStream();
				ulong decompressedPayloadOffset = 0;

				bool willHaveBlocks = header.Method != CompressedBufferHeader.CompressionMethod.None;
				if (willHaveBlocks)
				{
					ulong blockSize = 1ul << header.BlockSizeExponent;

					foreach (uint compressedBlockSize in compressedBlockSizes)
					{
						ulong rawBlockSize = Math.Min(header.TotalRawSize - decompressedPayloadOffset, blockSize);
						byte[] compressedPayload = br.ReadBytes((int)compressedBlockSize);

						int writtenBytes;
						// if a block has the same raw and compressed size its uncompressed and we should not attempt to decompress it
						if (rawBlockSize == compressedBlockSize)
						{
							writtenBytes = (int)rawBlockSize;
							targetStream.Write(compressedPayload);
						}
						else
						{
							writtenBytes = DecompressPayload(compressedPayload, header, rawBlockSize, targetStream);
						}

						decompressedPayloadOffset += (uint)writtenBytes;
					}
				}
				else
				{
					await sourceStream.CopyToAsync(targetStream, cancellationToken);
				}
			}

			// not using the buffered payload as we transfer the ownership to the caller of this method
			FilesystemBufferedPayload? finalizedBufferedPayload = null;
			try
			{
#pragma warning disable CA2000
				finalizedBufferedPayload = bufferedPayloadWriter.Done();
#pragma warning restore CA2000

				if (header.TotalRawSize != (ulong)finalizedBufferedPayload.Length)
				{
					throw new Exception("Did not decompress the full payload");
				}

				{
					using TelemetrySpan _ = _tracer.StartActiveSpan("web.hash").SetAttribute("operation.name", "web.hash");

					// only read the first 20 bytes of the hash field as IoHashes are 20 bytes and not 32 bytes
					byte[] slicedHash = new byte[20];
					Array.Copy(header.RawHash, 0, slicedHash, 0, 20);

					BlobId headerIdentifier = new BlobId(slicedHash);
					await using Stream hashStream = finalizedBufferedPayload.GetStream();
					BlobId contentHash = await BlobId.FromStreamAsync(hashStream, cancellationToken);

					if (!headerIdentifier.Equals(contentHash))
					{
						throw new Exception($"Payload was expected to be {headerIdentifier} but was {contentHash}");
					}
				}

				return finalizedBufferedPayload;
			}
			catch
			{
				finalizedBufferedPayload?.Dispose();
				throw;
			}
		}

		private static int DecompressPayload(ReadOnlySpan<byte> compressedPayload, CompressedBufferHeader header, ulong rawBlockSize, Stream target)
		{
			switch (header.Method)
			{
				case CompressedBufferHeader.CompressionMethod.None:
					target.Write(compressedPayload);
					return compressedPayload.Length;
				case CompressedBufferHeader.CompressionMethod.Oodle:
					{
						byte[] result = new byte[rawBlockSize];
						long writtenBytes = Oodle.Decompress(compressedPayload, result);
						if (writtenBytes == 0)
						{
							throw new Exception("Failed to run oodle decompress");
						}
						target.Write(result);
						return (int)writtenBytes;
					}
				case CompressedBufferHeader.CompressionMethod.LZ4:
					{
						byte[] result = new byte[rawBlockSize];
						int writtenBytes = LZ4Codec.Decode(compressedPayload, result);
						target.Write(result);
						return writtenBytes;
					}
				default:
					throw new NotImplementedException($"Method {header.Method} is not a support value");
			}
		}

		public IoHash CompressContent(Stream s, OoodleCompressorMethod method, OoodleCompressionLevel compressionLevel, byte[] rawContents)
		{
			const long DefaultBlockSize = 256 * 1024;
			long blockSize = DefaultBlockSize;
			long blockCount = (rawContents.LongLength + blockSize - 1) / blockSize;
			Span<byte> contentsSpan = new Span<byte>(rawContents);
			List<byte[]> blocks = new List<byte[]>();

			for (int i = 0; i < blockCount; i++)
			{
				int rawBlockSize = Math.Min(rawContents.Length - (i * (int)blockSize), (int)blockSize);
				Span<byte> bufferToCompress = contentsSpan.Slice((int)(i * blockSize), rawBlockSize);

				blocks.Add(bufferToCompress.ToArray());
			}

			return CompressContent(s, method, compressionLevel, blocks, blockSize);
		}

		public IoHash CompressContent(Stream s, OoodleCompressorMethod method, OoodleCompressionLevel compressionLevel, List<byte[]> blocks, long blockSize)
		{
			OodleCompressorType oodleMethod = OodleUtils.ToOodleApiCompressor(method);
			OodleCompressionLevel oodleLevel = OodleUtils.ToOodleApiCompressionLevel(compressionLevel);

			long blockCount = blocks.Count;

			byte blockSizeExponent = (byte)Math.Floor(Math.Log2(blockSize));

			//Span<byte> contentsSpan = new Span<byte>(rawContents);
			List<byte[]> compressedBlocks = new List<byte[]>();
			using Hasher hasher = Hasher.New();

			ulong uncompressedContentLength = (ulong)blocks.Sum(b => b.LongLength);

			ulong compressedContentLength = 0;
			for (int i = 0; i < blockCount; i++)
			{
				int rawBlockSize = blocks[i].Length;
				//int rawBlockSize = Math.Min(rawContents.Length - (i * (int)blockSize), (int)blockSize);
				//Span<byte> bufferToCompress = contentsSpan.Slice((int)(i * blockSize), rawBlockSize);
				byte[] bufferToCompress = blocks[i];
				hasher.UpdateWithJoin(new ReadOnlySpan<byte>(bufferToCompress, 0, rawBlockSize));
				int maxSize = Oodle.MaximumOutputSize(oodleMethod, rawBlockSize);
				byte[] compressedBlock = new byte[maxSize];
				long encodedSize = Oodle.Compress(oodleMethod, bufferToCompress, compressedBlock, oodleLevel);

				if (encodedSize == 0)
				{
					throw new Exception("Failed to compress content");
				}

				byte[] actualCompressedBlock = new byte[encodedSize];
				Array.Copy(compressedBlock, actualCompressedBlock, encodedSize);
				compressedBlocks.Add(actualCompressedBlock);
				compressedContentLength += (ulong)encodedSize;
			}

			Hash blake3Hash = hasher.Finalize();
			byte[] hashData = blake3Hash.AsSpanUnsafe().Slice(0, 20).ToArray();
			IoHash hash = new IoHash(hashData);

			CompressedBufferHeader header = new CompressedBufferHeader
			{
				Magic = CompressedBufferHeader.ExpectedMagic,
				Crc32 = 0,
				Method = CompressedBufferHeader.CompressionMethod.Oodle,
				CompressionLevel = (byte)compressionLevel,
				CompressionMethodUsed = (byte)method,
				BlockSizeExponent = blockSizeExponent,
				BlockCount = (uint)blockCount,
				TotalRawSize = (ulong)uncompressedContentLength,
				TotalCompressedSize = (ulong)compressedContentLength,
				RawHash = hashData
			};

			byte[] headerAndBlocks = WriteHeaderToBuffer(header, compressedBlocks.Select(b => (uint)b.Length).ToArray());

			using BinaryWriter writer = new BinaryWriter(s, Encoding.Default, leaveOpen: true);

			writer.Write(headerAndBlocks);

			for (int i = 0; i < blockCount; i++)
			{
				writer.Write(compressedBlocks[i]);
			}

			return hash;
		}

		public static byte[] WriteHeaderToBuffer(CompressedBufferHeader header, uint[] compressedBlockLengths)
		{
			uint blockCount = header.BlockCount;
			uint blocksByteUsed = blockCount * sizeof(uint);

			byte[] headerBuffer = new byte[CompressedBufferHeader.HeaderLength + blocksByteUsed];

			// write the compressed buffer, but with the wrong crc which we update and rewrite later
			{
				using MemoryStream ms = new MemoryStream(headerBuffer);
				using BinaryWriter writer = new BinaryWriter(ms);

				WriteHeader(header, writer);

				for (int i = 0; i < blockCount; i++)
				{
					uint value = compressedBlockLengths[i];
					if (BitConverter.IsLittleEndian)
					{
						value = BinaryPrimitives.ReverseEndianness(value);
					}
					writer.Write(value);
				}
			}

			// calculate the crc from the start of the method field (skipping magic which is a constant and the crc field itself)
			const int MethodOffset = sizeof(uint) + sizeof(uint);
			uint calculatedCrc = Crc32Algorithm.Compute(headerBuffer, MethodOffset, (int)(CompressedBufferHeader.HeaderLength - MethodOffset + blocksByteUsed));
			header.Crc32 = calculatedCrc;

			// write the header again now that we have the crc
			{
				using MemoryStream ms = new MemoryStream(headerBuffer);
				using BinaryWriter writer = new BinaryWriter(ms);
				WriteHeader(header, writer);
			}

			return headerBuffer;
		}
	}

	public class InvalidHashException : Exception
	{
		public InvalidHashException(uint headerCrc32, uint calculatedCrc) : base($"Header specified crc \"{headerCrc32}\" but calculated hash was \"{calculatedCrc}\"")
		{

		}
	}

	public class InvalidMagicException : Exception
	{
		public InvalidMagicException(uint headerMagic, uint expectedMagic) : base($"Header magic \"{headerMagic}\" was incorrect, expected to be {expectedMagic}")
		{
		}
	}

	// from OodleDataCompression.h , we define our own enums for oodle compressions used and convert to the ones expected in the oodle api
#pragma warning disable CA1028 // Enum Storage should be Int32
	public enum OoodleCompressorMethod : byte

	{
		NotSet = 0,
		Selkie = 1,
		Mermaid = 2,
		Kraken = 3,
		Leviathan = 4
	}

	public enum OoodleCompressionLevel : sbyte
	{
		HyperFast4 = -4,
		HyperFast3 = -3,
		HyperFast2 = -2,
		HyperFast1 = -1,
		None = 0,
		SuperFast = 1,
		VeryFast = 2,
		Fast = 3,
		Normal = 4,
		Optimal1 = 5,
		Optimal2 = 6,
		Optimal3 = 7,
		Optimal4 = 8,
	}
#pragma warning restore CA1028 // Enum Storage should be Int32
	public static class OodleUtils
	{
		public static OodleCompressionLevel ToOodleApiCompressionLevel(OoodleCompressionLevel compressionLevel)
		{
			switch (compressionLevel)
			{
				case OoodleCompressionLevel.HyperFast4:
					return OodleCompressionLevel.HyperFast4;
				case OoodleCompressionLevel.HyperFast3:
					return OodleCompressionLevel.HyperFast3;
				case OoodleCompressionLevel.HyperFast2:
					return OodleCompressionLevel.HyperFast2;
				case OoodleCompressionLevel.HyperFast1:
					return OodleCompressionLevel.HyperFast1;
				case OoodleCompressionLevel.None:
					return OodleCompressionLevel.None;
				case OoodleCompressionLevel.SuperFast:
					return OodleCompressionLevel.SuperFast;
				case OoodleCompressionLevel.VeryFast:
					return OodleCompressionLevel.VeryFast;
				case OoodleCompressionLevel.Fast:
					return OodleCompressionLevel.Fast;
				case OoodleCompressionLevel.Normal:
					return OodleCompressionLevel.Normal;
				case OoodleCompressionLevel.Optimal1:
					return OodleCompressionLevel.Optimal1;
				case OoodleCompressionLevel.Optimal2:
					return OodleCompressionLevel.Optimal2;
				case OoodleCompressionLevel.Optimal3:
					return OodleCompressionLevel.Optimal3;
				case OoodleCompressionLevel.Optimal4:
					return OodleCompressionLevel.Optimal4;
				default:
					throw new ArgumentOutOfRangeException(nameof(compressionLevel), compressionLevel, null);
			}
		}

		public static OodleCompressorType ToOodleApiCompressor(OoodleCompressorMethod compressor)
		{
			switch (compressor)
			{
				case OoodleCompressorMethod.NotSet:
					return OodleCompressorType.None;
				case OoodleCompressorMethod.Selkie:
					return OodleCompressorType.Selkie;
				case OoodleCompressorMethod.Mermaid:
					return OodleCompressorType.Mermaid;
				case OoodleCompressorMethod.Kraken:
					return OodleCompressorType.Kraken;
				case OoodleCompressorMethod.Leviathan:
					return OodleCompressorType.Leviathan;
				default:
					throw new ArgumentOutOfRangeException(nameof(compressor), compressor, null);
			}
		}
	}
}
