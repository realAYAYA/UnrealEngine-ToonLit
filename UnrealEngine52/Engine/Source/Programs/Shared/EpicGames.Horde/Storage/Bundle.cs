// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using K4os.Compression.LZ4;
using System;
using System.Buffers;
using System.Buffers.Binary;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Bundle version number
	/// </summary>
	[System.Diagnostics.CodeAnalysis.SuppressMessage("Design", "CA1027:Mark enums with FlagsAttribute")]
	public enum BundleVersion
	{
		/// <summary>
		/// Initial version number
		/// </summary>
		Initial = 0,

		/// <summary>
		/// Added the <see cref="BundleExport.Alias"/> property
		/// </summary>
		ExportAliases = 1,

		/// <summary>
		/// Last item in the enum. Used for <see cref="Latest"/>
		/// </summary>
		LatestPlusOne,

#pragma warning disable CA1069 // Enums values should not be duplicated
		/// <summary>
		/// The current version number
		/// </summary>
		Latest = (int)LatestPlusOne - 1,
#pragma warning restore CA1069 // Enums values should not be duplicated
	}

	/// <summary>
	/// Header for the contents of a bundle. May contain an inlined payload object containing the object data itself.
	/// </summary>
	public class Bundle
	{
		/// <summary>
		/// Header for the bundle
		/// </summary>
		public BundleHeader Header { get; }

		/// <summary>
		/// Packet data as described in the header
		/// </summary>
		public IReadOnlyList<ReadOnlyMemory<byte>> Packets { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public Bundle(BundleHeader header, IReadOnlyList<ReadOnlyMemory<byte>> packets)
		{
			Header = header;
			Packets = packets;
		}

		/// <summary>
		/// Reads a bundle from a block of memory
		/// </summary>
		public Bundle(IMemoryReader reader)
		{
			Header = new BundleHeader(reader);

			ReadOnlyMemory<byte>[] packets = new ReadOnlyMemory<byte>[Header.Packets.Count];
			for (int idx = 0; idx < Header.Packets.Count; idx++)
			{
				BundlePacket packet = Header.Packets[idx];
				packets[idx] = reader.ReadFixedLengthBytes(packet.EncodedLength);
			}

			Packets = packets;
		}

		/// <summary>
		/// Reads a bundle from the given stream
		/// </summary>
		/// <param name="stream">Stream to read from</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Bundle that was read</returns>
		public static async Task<Bundle> FromStreamAsync(Stream stream, CancellationToken cancellationToken)
		{
			BundleHeader header = await BundleHeader.FromStreamAsync(stream, cancellationToken);

			ReadOnlyMemory<byte>[] packets = new ReadOnlyMemory<byte>[header.Packets.Count];
			for (int idx = 0; idx < header.Packets.Count; idx++)
			{
				BundlePacket packet = header.Packets[idx];

				byte[] data = new byte[packet.EncodedLength];
				await stream.ReadFixedLengthBytesAsync(data, cancellationToken);

				packets[idx] = data;
			}

			return new Bundle(header, packets);
		}

		/// <summary>
		/// Serializes the bundle to a sequence of bytes
		/// </summary>
		/// <returns>Sequence for the bundle</returns>
		public ReadOnlySequence<byte> AsSequence()
		{
			ByteArrayBuilder builder = new ByteArrayBuilder();
			Header.Write(builder);

			ReadOnlySequenceBuilder<byte> sequence = new ReadOnlySequenceBuilder<byte>();
			sequence.Append(builder.AsSequence());

			foreach (ReadOnlyMemory<byte> packet in Packets)
			{
				sequence.Append(packet);
			}

			return sequence.Construct();
		}
	}

	/// <summary>
	/// Indicates the compression format in the bundle
	/// </summary>
	public enum BundleCompressionFormat
	{
		/// <summary>
		/// Packets are uncompressed
		/// </summary>
		None = 0,

		/// <summary>
		/// LZ4 compression
		/// </summary>
		LZ4 = 1,

		/// <summary>
		/// Gzip compression
		/// </summary>
		Gzip = 2,

		/// <summary>
		/// Oodle compression (Kraken)
		/// </summary>
		Oodle = 3,
	}

	/// <summary>
	/// Header for the contents of a bundle.
	/// </summary>
	public class BundleHeader
	{
		/// <summary>
		/// Signature bytes
		/// </summary>
		public static ReadOnlyMemory<byte> Signature { get; } = Encoding.UTF8.GetBytes("UEBN");

		/// <summary>
		/// Length of the prelude data
		/// </summary>
		public const int PreludeLength = 8;

		/// <summary>
		/// Compression format for the bundle
		/// </summary>
		public BundleCompressionFormat CompressionFormat { get; }

		/// <summary>
		/// Types for exports within this bundle
		/// </summary>
		public IReadOnlyList<BundleType> Types { get; }

		/// <summary>
		/// References to nodes in other bundles
		/// </summary>
		public IReadOnlyList<BundleImport> Imports { get; }

		/// <summary>
		/// Nodes exported from this bundle
		/// </summary>
		public IReadOnlyList<BundleExport> Exports { get; }

		/// <summary>
		/// List of data packets within this bundle
		/// </summary>
		public IReadOnlyList<BundlePacket> Packets { get; }

		/// <summary>
		/// Constructs a new bundle header
		/// </summary>
		/// <param name="compressionFormat">Compression format for bundle packets</param>
		/// <param name="types">Type array indexed by each export</param>
		/// <param name="imports">Imports from other bundles</param>
		/// <param name="exports">Exports for nodes</param>
		/// <param name="packets">Compression packets within the bundle</param>
		public BundleHeader(BundleCompressionFormat compressionFormat, IReadOnlyList<BundleType> types, IReadOnlyList<BundleImport> imports, IReadOnlyList<BundleExport> exports, IReadOnlyList<BundlePacket> packets)
		{
			CompressionFormat = compressionFormat;
			Types = types;
			Imports = imports;
			Exports = exports;
			Packets = packets;
		}

		/// <summary>
		/// Reads a bundle header from memory
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>New header object</returns>
		public BundleHeader(IMemoryReader reader)
		{
			ReadOnlyMemory<byte> prelude = reader.GetMemory(8);
			CheckPrelude(prelude);
			reader.Advance(8);

			BundleVersion version = (BundleVersion)reader.ReadUnsignedVarInt();
			if (version > BundleVersion.Latest)
			{
				throw new InvalidDataException($"Unknown bundle version {(int)version}. Max supported is {(int)BundleVersion.Latest}.");
			}

			CompressionFormat = (BundleCompressionFormat)reader.ReadUnsignedVarInt();
			Types = reader.ReadVariableLengthArray(() => new BundleType(reader));
			Imports = reader.ReadVariableLengthArray(() => new BundleImport(reader));
			Exports = reader.ReadVariableLengthArray(() => new BundleExport(reader, version));

			if (CompressionFormat == BundleCompressionFormat.None)
			{
				Packets = Exports.ConvertAll(x => new BundlePacket(x.Length, x.Length));
			}
			else
			{
				Packets = reader.ReadVariableLengthArray(() => new BundlePacket(reader));
			}
		}

		/// <summary>
		/// Reads the prelude bytes from a bundle, and returns the header size.
		/// </summary>
		/// <param name="prelude">The prelude data</param>
		/// <returns>Size of the header, including the prelude data</returns>
		public static int ReadPrelude(ReadOnlyMemory<byte> prelude)
		{
			CheckPrelude(prelude);
			return BinaryPrimitives.ReadInt32BigEndian(prelude.Span.Slice(4));
		}

		/// <summary>
		/// Reads a bundle header from a stream
		/// </summary>
		/// <param name="stream">Stream to read from</param>
		/// <param name="cancellationToken">Cancellation token for the stream</param>
		/// <returns>New header</returns>
		public static async Task<BundleHeader> FromStreamAsync(Stream stream, CancellationToken cancellationToken)
		{
			byte[] prelude = new byte[8];
			await stream.ReadFixedLengthBytesAsync(prelude, cancellationToken);
			CheckPrelude(prelude);

			int headerLength = BinaryPrimitives.ReadInt32BigEndian(prelude.AsSpan(4));
			using (IMemoryOwner<byte> owner = MemoryPool<byte>.Shared.Rent(headerLength))
			{
				Memory<byte> header = owner.Memory.Slice(0, headerLength);
				prelude.CopyTo(header);

				await stream.ReadFixedLengthBytesAsync(header.Slice(prelude.Length), cancellationToken);
				return new BundleHeader(new MemoryReader(header));
			}
		}

		/// <summary>
		/// Validates that the prelude bytes for a bundle header are correct
		/// </summary>
		/// <param name="prelude">The prelude bytes</param>
		static void CheckPrelude(ReadOnlyMemory<byte> prelude)
		{
			if (!prelude.Slice(0, 4).Span.SequenceEqual(Signature.Span))
			{
				throw new InvalidDataException("Invalid signature bytes for bundle. Corrupt data?");
			}
		}

		/// <summary>
		/// Serializes a header to memory
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		public void Write(IMemoryWriter writer)
		{
			int initialLength = writer.Length;

			// Write the header prelude; a fixed-length block containing a signature and header size.
			Memory<byte> prelude = writer.GetMemory(8);
			Signature.CopyTo(prelude);
			writer.Advance(8);

			// Write the header data
			writer.WriteUnsignedVarInt((ulong)BundleVersion.Latest);

			writer.WriteUnsignedVarInt((ulong)CompressionFormat);
			writer.WriteVariableLengthArray(Types, x => x.Write(writer));
			writer.WriteVariableLengthArray(Imports, x => x.Write(writer));
			writer.WriteVariableLengthArray(Exports, x => x.Write(writer));

			if (CompressionFormat != BundleCompressionFormat.None)
			{
				writer.WriteVariableLengthArray(Packets, x => x.Write(writer));
			}

			// Go back and write the length of the header
			BinaryPrimitives.WriteInt32BigEndian(prelude.Slice(4).Span, writer.Length - initialLength);
		}
	}

	/// <summary>
	/// Information about a type within a bundle
	/// </summary>
	[DebuggerDisplay("{Guid}#{Version}")]
	public class BundleType : IEquatable<BundleType>
	{
		/// <summary>
		/// Nominal identifier for the type
		/// </summary>
		public Guid Guid { get; }

		/// <summary>
		/// Version number for the serializer
		/// </summary>
		public int Version { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="guid">Nominal identifier for the type</param>
		/// <param name="version">Version number for the serializer</param>
		public BundleType(Guid guid, int version)
		{
			Guid = guid;
			Version = version;
		}

		/// <summary>
		/// Deserialize a bundle type
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		public BundleType(IMemoryReader reader)
		{
			Guid = reader.ReadGuid();
			Version = (int)reader.ReadUnsignedVarInt();
		}

		/// <summary>
		/// Serializes a type to storage
		/// </summary>
		/// <param name="writer">Writer for serialization</param>
		public void Write(IMemoryWriter writer)
		{
			writer.WriteGuid(Guid);
			writer.WriteUnsignedVarInt(Version);
		}

		/// <summary>
		/// Parse a span of characters as a bundle type
		/// </summary>
		/// <param name="text">Text to parse</param>
		/// <returns>The parsed bundle type</returns>
		public static BundleType Parse(ReadOnlySpan<char> text)
		{
			int hashIdx = text.IndexOf('#');
			Guid guid = Guid.Parse(text.Slice(0, hashIdx));
			int version = Int32.Parse(text.Slice(hashIdx + 1), NumberStyles.None, CultureInfo.InvariantCulture);
			return new BundleType(guid, version);
		}

		/// <inheritdoc/>
		public override bool Equals(object? obj) => obj is BundleType type && Equals(type);

		/// <inheritdoc/>
		public override int GetHashCode() => HashCode.Combine(Guid, Version);

		/// <inheritdoc/>
		public bool Equals(BundleType? type) => type is object && Guid == type.Guid && Version == type.Version;

		/// <inheritdoc/>
		public override string ToString() => $"{Guid}#{Version}";

		/// <inheritdoc/>
		public static bool operator ==(BundleType lhs, BundleType rhs) => lhs.Equals(rhs);

		/// <inheritdoc/>
		public static bool operator !=(BundleType lhs, BundleType rhs) => !lhs.Equals(rhs);
	}

	/// <summary>
	/// Reference to nodes in another bundle
	/// </summary>
	public class BundleImport
	{
		/// <summary>
		/// Blob containing the bundle data.
		/// </summary>
		public BlobLocator Locator { get; }

		/// <summary>
		/// Entries for each node
		/// </summary>
		public IReadOnlyList<int> Exports { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleImport(BlobLocator locator, IReadOnlyList<int> exports)
		{
			Locator = locator;
			Exports = exports;
		}

		/// <summary>
		/// Deserialize a bundle import
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		internal BundleImport(IMemoryReader reader)
		{
			Locator = reader.ReadBlobLocator();
			Exports = reader.ReadVariableLengthArray(() => (int)reader.ReadUnsignedVarInt());
		}

		/// <summary>
		/// Serializes a bundle import
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		public void Write(IMemoryWriter writer)
		{
			writer.WriteBlobLocator(Locator);
			writer.WriteVariableLengthArray(Exports, x => writer.WriteUnsignedVarInt(x));
		}
	}

	/// <summary>
	/// Descriptor for a compression packet
	/// </summary>
	public class BundlePacket
	{
		/// <summary>
		/// Encoded length of the packet
		/// </summary>
		public int EncodedLength { get; }

		/// <summary>
		/// Decoded length of the packet
		/// </summary>
		public int DecodedLength { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="encodedLength">Size of the encoded data</param>
		/// <param name="decodedLength">Size of the decoded data</param>
		public BundlePacket(int encodedLength, int decodedLength)
		{
			EncodedLength = encodedLength;
			DecodedLength = decodedLength;
		}

		/// <summary>
		/// Deserialize a packet header from a memory reader
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		public BundlePacket(IMemoryReader reader)
		{
			EncodedLength = (int)reader.ReadUnsignedVarInt();
			DecodedLength = (int)reader.ReadUnsignedVarInt();
		}

		/// <summary>
		/// Serializes this packet header 
		/// </summary>
		/// <param name="writer"></param>
		public void Write(IMemoryWriter writer)
		{
			writer.WriteUnsignedVarInt(EncodedLength);
			writer.WriteUnsignedVarInt(DecodedLength);
		}
	}

	/// <summary>
	/// Entry for a node exported from an object
	/// </summary>
	public class BundleExport
	{
		/// <summary>
		/// Index of the type within the type list
		/// </summary>
		public int TypeIdx { get; }

		/// <summary>
		/// Hash of the node
		/// </summary>
		public IoHash Hash { get; }

		/// <summary>
		/// Uncompressed length of this node
		/// </summary>
		public int Length { get; }

		/// <summary>
		/// Nodes referenced by this export. Indices in this array correspond to a lookup table consisting
        /// of the imported nodes in the order they are declared in the header, followed by nodes listed in the
        /// export table itself.
        /// </summary>
		public IReadOnlyList<int> References { get; }

		/// <summary>
		/// Alias for the content of this node. Used for content addressing.
		/// </summary>
		public Utf8String Alias { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleExport(int typeIdx, IoHash hash, int length, IReadOnlyList<int> references, Utf8String alias = default)
		{
			TypeIdx = typeIdx;
			Hash = hash;
			Length = length;
			References = references;
			Alias = alias;
		}

		/// <summary>
		/// Deserialize an export
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <param name="version">Version number of the bundle</param>
		public BundleExport(IMemoryReader reader, BundleVersion version)
		{
			TypeIdx = (int)reader.ReadUnsignedVarInt();
			Hash = reader.ReadIoHash();
			Length = (int)reader.ReadUnsignedVarInt();

			int numReferences = (int)reader.ReadUnsignedVarInt();
			if (numReferences == 0)
			{
				References = Array.Empty<int>();
			}
			else
			{
				int[] references = new int[numReferences];
				for (int idx = 0; idx < numReferences; idx++)
				{
					references[idx] = (int)reader.ReadUnsignedVarInt();
				}
				References = references;
			}

			if (version >= BundleVersion.ExportAliases)
			{
				Alias = reader.ReadUtf8String();
			}
		}

		/// <summary>
		/// Serializes an export
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		public void Write(IMemoryWriter writer)
		{
			writer.WriteUnsignedVarInt(TypeIdx);
			writer.WriteIoHash(Hash);
			writer.WriteUnsignedVarInt(Length);

			writer.WriteUnsignedVarInt(References.Count);
			for(int idx = 0; idx < References.Count; idx++)
			{
				writer.WriteUnsignedVarInt(References[idx]);
			}

			writer.WriteUtf8String(Alias);
		}
	}

	/// <summary>
	/// Utility methods for bundles
	/// </summary>
	public static class BundleData
	{
		/// <summary>
		/// Compress a data packet
		/// </summary>
		/// <param name="format">Format for the compressed data</param>
		/// <param name="input">The data to compress</param>
		/// <param name="writer">Writer for output data</param>
		/// <returns>The compressed data</returns>
		public static int Compress(BundleCompressionFormat format, ReadOnlyMemory<byte> input, IMemoryWriter writer)
		{
			switch (format)
			{
				case BundleCompressionFormat.None:
					{
						writer.WriteFixedLengthBytes(input.Span);
						return input.Length;
					}
				case BundleCompressionFormat.LZ4:
					{
						int maxSize = LZ4Codec.MaximumOutputSize(input.Length);

						Span<byte> buffer = writer.GetSpan(maxSize);
						int encodedLength = LZ4Codec.Encode(input.Span, buffer);

						writer.Advance(encodedLength);
						return encodedLength;
					}
				case BundleCompressionFormat.Gzip:
					{
						using MemoryStream outputStream = new MemoryStream(input.Length);
						using GZipStream gzipStream = new GZipStream(outputStream, CompressionLevel.Fastest);
						gzipStream.Write(input.Span);

						writer.WriteFixedLengthBytes(outputStream.ToArray());
						return (int)outputStream.Length;
					}
				case BundleCompressionFormat.Oodle:
					{
#if WITH_OODLE
						int maxSize = Oodle.MaximumOutputSize(OodleCompressorType.Selkie, input.Length);

						Span<byte> outputSpan = builder.GetSpan(maxSize);
						int encodedLength = Oodle.Compress(OodleCompressorType.Selkie, input.Span, outputSpan, OodleCompressionLevel.HyperFast);
						builder.Advance(encodedLength);

						return encodedLength;
#else
						throw new NotSupportedException("Oodle is not compiled into this build.");
#endif
					}
				default:
					throw new InvalidDataException($"Invalid compression format '{(int)format}'");
			}
		}

		/// <summary>
		/// Decompress a packet of data
		/// </summary>
		/// <param name="format">Format of the compressed data</param>
		/// <param name="input">Compressed data</param>
		/// <param name="output">Buffer to receive the decompressed data</param>
		public static void Decompress(BundleCompressionFormat format, ReadOnlyMemory<byte> input, Memory<byte> output)
		{
			switch (format)
			{
				case BundleCompressionFormat.None:
					input.CopyTo(output);
					break;
				case BundleCompressionFormat.LZ4:
					LZ4Codec.Decode(input.Span, output.Span);
					break;
				case BundleCompressionFormat.Gzip:
					{
						using ReadOnlyMemoryStream inputStream = new ReadOnlyMemoryStream(input);
						using GZipStream outputStream = new GZipStream(new MemoryWriterStream(new MemoryWriter(output)), CompressionMode.Decompress, false);
						inputStream.CopyTo(outputStream);
						break;
					}
				case BundleCompressionFormat.Oodle:
#if WITH_OODLE
					Oodle.Decompress(input.Span, output.Span);
					break;
#else
					throw new NotSupportedException("Oodle is not compiled into this build.");
#endif
				default:
					throw new InvalidDataException($"Invalid compression format '{(int)format}'");
			}
		}
	}
}
