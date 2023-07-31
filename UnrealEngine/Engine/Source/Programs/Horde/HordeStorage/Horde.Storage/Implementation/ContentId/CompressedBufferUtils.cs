// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Datadog.Trace;
using EpicGames.Core;
using Force.Crc32;
using Jupiter.Implementation;
using Jupiter.Utils;
using K4os.Compression.LZ4;

namespace Horde.Storage.Implementation
{
    public class CompressedBufferUtils
    {
        private readonly OodleCompressor _oodleCompressor;

        public CompressedBufferUtils(OodleCompressor oodleCompressor)
        {
            _oodleCompressor = oodleCompressor;
        }

        private class Header
        {
            [System.Diagnostics.CodeAnalysis.SuppressMessage("Design", "CA1028:Enum Storage should be Int32", Justification = "Interop requires byte")]
            public enum CompressionMethod : byte
            {
                // Header is followed by one uncompressed block. 
                None = 0,
                // Header is followed by an array of compressed block sizes then the compressed blocks. 
                Oodle=3,
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

        private static Header ExtractHeader(byte[] content)
        {
            // the header is always stored big endian
            bool needsByteSwap = BitConverter.IsLittleEndian;
            if (content.Length < Header.HeaderLength)
            {
                throw new ArgumentOutOfRangeException(nameof(content), $"Content was less then {Header.HeaderLength} bytes and thus is not a compressed buffer");
            }

            using MemoryStream ms = new MemoryStream(content);
            using BinaryReader reader = new BinaryReader(ms);

            Header header = new Header
            {
                Magic = reader.ReadUInt32(),
                Crc32 = reader.ReadUInt32(),
                Method = (Header.CompressionMethod) reader.ReadByte(),
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

            if (ms.Position != Header.HeaderLength)
            {
                throw new Exception($"Read {ms.Position} bytes but expected to read {Header.HeaderLength}");
            }

            if (header.Magic != Header.ExpectedMagic)
            {
                throw new InvalidMagicException(header.Magic, Header.ExpectedMagic);
            }

            // calculate the crc from the start of the method field (skipping magic which is a constant and the crc field itself)
            const int MethodOffset = sizeof(uint) + sizeof(uint);

            // none compressed objects have no extra blocks
            uint blocksByteUsed = header.Method != Header.CompressionMethod.None ? header.BlockCount * (uint)sizeof(uint) : 0;
            uint calculatedCrc = Crc32Algorithm.Compute(content, MethodOffset, (int)(Header.HeaderLength - MethodOffset + blocksByteUsed));
            
            if (header.Crc32 != calculatedCrc)
            {
                throw new InvalidHashException(header.Crc32, calculatedCrc);
            }

            return header;
        }

        static void WriteHeader(Header header, BinaryWriter writer)
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

        public byte[] DecompressContent(byte[] content)
        {
            Header header = ExtractHeader(content);

            if (content.LongLength < (long)header.TotalCompressedSize)
            {
                throw new Exception($"Expected buffer to be {header.TotalCompressedSize} but it was {content.LongLength}");
            }

            ulong decompressedPayloadOffset = 0;
            byte[] decompressedPayload = new byte[header.TotalRawSize];

            ReadOnlySpan<byte> memory = new ReadOnlySpan<byte>(content);
            memory = memory.Slice((int)Header.HeaderLength);

            bool willHaveBlocks = header.Method != Header.CompressionMethod.None;
            if (willHaveBlocks)
            {
                uint[] compressedBlockSizes = new uint[header.BlockCount];
                for (int i = 0; i < header.BlockCount; i++)
                {
                    uint compressedBlockSize = BinaryPrimitives.ReadUInt32BigEndian(memory);
                    compressedBlockSizes[i] = compressedBlockSize;
                    memory = memory.Slice(sizeof(uint));
                }

                ulong blockSize = 1ul << header.BlockSizeExponent;
                ulong compressedOffset = 0;

                foreach (uint compressedBlockSize in compressedBlockSizes)
                {
                    ulong rawBlockSize = Math.Min(header.TotalRawSize - decompressedPayloadOffset, blockSize);
                    ReadOnlySpan<byte> compressedPayload = memory.Slice((int)compressedOffset, (int)compressedBlockSize);
                    Span<byte> targetSpan = new Span<byte>(decompressedPayload, (int)decompressedPayloadOffset, (int)rawBlockSize);

                    int writtenBytes;
                    // if a block has the same raw and compressed size its uncompressed and we should not attempt to decompress it
                    if (rawBlockSize == compressedBlockSize)
                    {
                        writtenBytes = (int)rawBlockSize;
                        compressedPayload.CopyTo(targetSpan);
                    }
                    else
                    {
                        writtenBytes = DecompressPayload(compressedPayload, header, rawBlockSize, targetSpan);
                    }

                    decompressedPayloadOffset += (uint)writtenBytes;
                    compressedOffset += compressedBlockSize;
                }
            }
            else
            {
                // if no compression is applied there are no extra blocks and just a single chunk that is uncompressed
                Span<byte> targetSpan = new Span<byte>(decompressedPayload, 0, (int)header.TotalRawSize);
                DecompressPayload(memory, header, header.TotalRawSize, targetSpan);
                decompressedPayloadOffset = header.TotalRawSize;
            }
           
            if (header.TotalRawSize != decompressedPayloadOffset)
            {
                throw new Exception("Did not decompress the full payload");
            }

            {
                using IScope _ = Tracer.Instance.StartActive("web.hash");

                // only read the first 20 bytes of the hash field as IoHashes are 20 bytes and not 32 bytes
                byte[] slicedHash = new byte[20];
                Array.Copy(header.RawHash, 0, slicedHash, 0, 20);

                BlobIdentifier headerIdentifier = new BlobIdentifier(slicedHash);
                BlobIdentifier contentHash = BlobIdentifier.FromBlob(decompressedPayload);
               
                if (!headerIdentifier.Equals(contentHash))
                {
                    throw new Exception($"Payload was expected to be {headerIdentifier} but was {contentHash}");
                }
            }

            return decompressedPayload;
        }

        private int DecompressPayload(ReadOnlySpan<byte> compressedPayload, Header header, ulong rawBlockSize, Span<byte> target)
        {
             switch (header.Method)
            {
                case Header.CompressionMethod.None:
                    compressedPayload.CopyTo(target);
                    return compressedPayload.Length;
                case Header.CompressionMethod.Oodle:
                {
                    long writtenBytes = _oodleCompressor.Decompress(compressedPayload.ToArray(), (long)rawBlockSize, out byte[] result);
                    if (writtenBytes == 0)
                    {
                        throw new Exception("Failed to run oodle decompress");
                    }
                    result.CopyTo(target);
                    return (int)writtenBytes;
                }
                case Header.CompressionMethod.LZ4:
                {
                    int writtenBytes = LZ4Codec.Decode(compressedPayload, target);
                    return writtenBytes;
                }
                default:
                    throw new NotImplementedException($"Method {header.Method} is not a support value");
            }
        }

        public byte[] CompressContent(OoodleCompressorMethod method, OoodleCompressionLevel compressionLevel, byte[] rawContents)
        {
            OodleLZ_Compressor oodleMethod = OodleUtils.ToOodleApiCompressor(method);
            OodleLZ_CompressionLevel oodleLevel = OodleUtils.ToOodleApiCompressionLevel(compressionLevel);

            const long DefaultBlockSize = 256 * 1024;
            long blockSize = DefaultBlockSize;
            long blockCount = (rawContents.LongLength + blockSize - 1) / blockSize;

            byte blockSizeExponent = (byte)Math.Floor(Math.Log2(blockSize));

            Span<byte> contentsSpan = new Span<byte>(rawContents);
            List<byte[]> compressedBlocks = new List<byte[]>();

            for (int i = 0; i < blockCount; i++)
            {
                int rawBlockSize = Math.Min(rawContents.Length - (i * (int)blockSize), (int)blockSize);
                Span<byte> bufferToCompress = contentsSpan.Slice((int)(i * blockSize), rawBlockSize);

                long encodedSize = _oodleCompressor.Compress(oodleMethod, bufferToCompress.ToArray(), oodleLevel, out byte[] compressedBlock);

                if (encodedSize == 0)
                {
                    throw new Exception("Failed to compress content");
                }

                compressedBlocks.Add(compressedBlock);
            }

            uint compressedContentLength = (uint)compressedBlocks.Sum(b => b.LongLength);

            Header header = new Header
            {
                Magic = Header.ExpectedMagic,
                Crc32 = 0,
                Method = Header.CompressionMethod.Oodle,
                CompressionLevel = (byte)compressionLevel,
                CompressionMethodUsed = (byte)method,
                BlockSizeExponent = blockSizeExponent,
                BlockCount = (uint)blockCount,
                TotalRawSize = (ulong)rawContents.LongLength,
                TotalCompressedSize = (ulong)compressedContentLength,
                RawHash = IoHash.Compute(rawContents).ToByteArray()
            };

            uint blocksByteUsed = (uint)blockCount * sizeof(uint);

            byte[] headerBuffer = new byte[Header.HeaderLength + blocksByteUsed + compressedContentLength];

            // write the compressed buffer, but with the wrong crc which we update and rewrite later
            {
                using MemoryStream ms = new MemoryStream(headerBuffer);
                using BinaryWriter writer = new BinaryWriter(ms);

                WriteHeader(header, writer);

                for (int i = 0; i < blockCount; i++)
                {
                    uint value = (uint)compressedBlocks[i].Length;
                    if (BitConverter.IsLittleEndian)
                    {
                        value = BinaryPrimitives.ReverseEndianness(value);
                    }
                    writer.Write(value);
                }

                for (int i = 0; i < blockCount; i++)
                {
                    writer.Write(compressedBlocks[i]);
                }
            }

            // calculate the crc from the start of the method field (skipping magic which is a constant and the crc field itself)
            const int MethodOffset = sizeof(uint) + sizeof(uint);
            uint calculatedCrc = Crc32Algorithm.Compute(headerBuffer, MethodOffset, (int)(Header.HeaderLength - MethodOffset + blocksByteUsed));
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
    public enum OoodleCompressorMethod: byte

    {
        NotSet = 0,
        Selkie = 1,
        Mermaid = 2,
        Kraken  = 3,
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
        public static OodleLZ_CompressionLevel ToOodleApiCompressionLevel(OoodleCompressionLevel compressionLevel)
        {
            switch (compressionLevel)
            {
                case OoodleCompressionLevel.HyperFast4:
                    return OodleLZ_CompressionLevel.OodleLZ_CompressionLevel_HyperFast4;
                case OoodleCompressionLevel.HyperFast3:
                    return OodleLZ_CompressionLevel.OodleLZ_CompressionLevel_HyperFast3;
                case OoodleCompressionLevel.HyperFast2:
                    return OodleLZ_CompressionLevel.OodleLZ_CompressionLevel_HyperFast2;
                case OoodleCompressionLevel.HyperFast1:
                    return OodleLZ_CompressionLevel.OodleLZ_CompressionLevel_HyperFast1;
                case OoodleCompressionLevel.None:
                    return OodleLZ_CompressionLevel.OodleLZ_CompressionLevel_None;
                case OoodleCompressionLevel.SuperFast:
                    return OodleLZ_CompressionLevel.OodleLZ_CompressionLevel_SuperFast;
                case OoodleCompressionLevel.VeryFast:
                    return OodleLZ_CompressionLevel.OodleLZ_CompressionLevel_VeryFast;
                case OoodleCompressionLevel.Fast:
                    return OodleLZ_CompressionLevel.OodleLZ_CompressionLevel_Fast;
                case OoodleCompressionLevel.Normal:
                    return OodleLZ_CompressionLevel.OodleLZ_CompressionLevel_Normal;
                case OoodleCompressionLevel.Optimal1:
                    return OodleLZ_CompressionLevel.OodleLZ_CompressionLevel_Optimal1;
                case OoodleCompressionLevel.Optimal2:
                    return OodleLZ_CompressionLevel.OodleLZ_CompressionLevel_Optimal2;
                case OoodleCompressionLevel.Optimal3:
                    return OodleLZ_CompressionLevel.OodleLZ_CompressionLevel_Optimal3;
                case OoodleCompressionLevel.Optimal4:
                    return OodleLZ_CompressionLevel.OodleLZ_CompressionLevel_Optimal4;
                default:
                    throw new ArgumentOutOfRangeException(nameof(compressionLevel), compressionLevel, null);
            }
        }

        public static OodleLZ_Compressor ToOodleApiCompressor(OoodleCompressorMethod compressor)
        {
            switch (compressor)
            {
                case OoodleCompressorMethod.NotSet:
                    return OodleLZ_Compressor.OodleLZ_Compressor_None;
                case OoodleCompressorMethod.Selkie:
                    return OodleLZ_Compressor.OodleLZ_Compressor_Selkie;
                case OoodleCompressorMethod.Mermaid:
                    return OodleLZ_Compressor.OodleLZ_Compressor_Mermaid;
                case OoodleCompressorMethod.Kraken:
                    return OodleLZ_Compressor.OodleLZ_Compressor_Kraken;
                case OoodleCompressorMethod.Leviathan:
                    return OodleLZ_Compressor.OodleLZ_Compressor_Leviathan;
                default:
                    throw new ArgumentOutOfRangeException(nameof(compressor), compressor, null);
            }
        }
    }
}
