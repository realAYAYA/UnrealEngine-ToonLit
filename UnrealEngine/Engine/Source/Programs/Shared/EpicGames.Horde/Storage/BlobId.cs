// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Serialization;
using System;
using System.Buffers.Binary;
using System.ComponentModel;
using System.Diagnostics;
using System.Globalization;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Globally unique identifier for a blob, as a utf-8 string. Clients should not assume any internal structure to this identifier.
	/// </summary>
	[JsonConverter(typeof(BlobIdJsonConverter))]
	[TypeConverter(typeof(BlobIdTypeConverter))]
	[CbConverter(typeof(BlobIdCbConverter))]
	public struct BlobId : IEquatable<BlobId>
	{
		/// <summary>
		/// Dummy enum to allow invoking the constructor which takes a sanitized full path
		/// </summary>
		public enum Validate
		{
			/// <summary>
			/// Dummy value
			/// </summary>
			None
		}

		/// <summary>
		/// Empty blob id
		/// </summary>
		public static BlobId Empty { get; } = default;

		/// <summary>
		/// Identifier for the ref
		/// </summary>
		public Utf8String Inner { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="inner"></param>
		public BlobId(Utf8String inner)
		{
			Inner = inner;
			ValidateArgument(nameof(inner), inner);
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="inner"></param>
		/// <param name="validate"></param>
		public BlobId(Utf8String inner, Validate validate)
		{
			Inner = inner;
			_ = validate;
		}

		static Utf8String s_process;
		static int s_counter;

		/// <summary>
		/// Static constructor
		/// </summary>
		[System.Diagnostics.CodeAnalysis.SuppressMessage("Usage", "CA2207:Initialize value type static fields inline")]
		static BlobId()
		{
			Random rnd = new Random(HashCode.Combine(DateTime.UtcNow.Ticks, Environment.ProcessId, Environment.TickCount64, Environment.MachineName.GetHashCode(StringComparison.Ordinal)));

			Span<byte> process = stackalloc byte[4];
			rnd.NextBytes(process);
			s_process = StringUtils.FormatUtf8HexString(process);

			s_counter = rnd.Next();
		}

		/// <summary>
		/// Create a unique blob id, optionally including a prefix
		/// </summary>
		/// <param name="prefix">Prefix for blob ids.</param>
		/// <returns>New content id</returns>
		public static BlobId CreateNew(Utf8String prefix = default)
		{
			byte[] buffer;
			Span<byte> span;

			if (prefix.Length > 0)
			{
				ValidateArgument(nameof(prefix), prefix);

				buffer = new byte[prefix.Length + 1 + 24];
				span = buffer;

				prefix.Span.CopyTo(span);
				span = span.Slice(prefix.Length);

				span[0] = (byte)'/';
				span = span.Slice(1);
			}
			else
			{
				buffer = new byte[24];
				span = buffer;
			}

			GenerateUniqueId(span);
			return new BlobId(new Utf8String(buffer), Validate.None);
		}

		/// <summary>
		/// Creates a new identifier in the given buffer.
		/// </summary>
		/// <param name="output">Buffer to receive the output. The first 24 bytes will be written to.</param>
		public static void GenerateUniqueId(Span<byte> output)
		{
			if (s_process.IsEmpty)
			{
				output.Slice(0, 16).Fill((byte)'0');
			}
			else
			{
				uint timestamp = (uint)((DateTime.UtcNow - DateTime.UnixEpoch).Ticks / TimeSpan.TicksPerSecond);
				StringUtils.FormatUtf8HexString(timestamp, output);

				s_process.Span.CopyTo(output.Slice(8));
			}

			uint counter = (uint)Interlocked.Increment(ref s_counter);
			StringUtils.FormatUtf8HexString(counter, output.Slice(16));
		}

		/// <summary>
		/// Enables the generation of deterministic ids, for tests
		/// </summary>
		/// <param name="counter">Next counter for new ids</param>
		public static void UseDeterministicIds(int counter = 0)
		{
			s_process = Utf8String.Empty;
			s_counter = counter;
		}

		/// <summary>
		/// Checks whether this blob id is valid
		/// </summary>
		/// <returns>True if the identifier is valid</returns>
		public bool IsValid() => Inner.Length > 0;

		/// <summary>
		/// Validates a given string as a blob id
		/// </summary>
		/// <param name="name">Name of the argument</param>
		/// <param name="text">String to validate</param>
		public static void ValidateArgument(string name, Utf8String text)
		{
			if (text.Length == 0)
			{
				throw new ArgumentException("Blob identifiers cannot be empty", name);
			}
			if (text[^1] == '/')
			{
				throw new ArgumentException("Blob identifiers cannot start or end with a slash", name);
			}

			int lastSlashIdx = -1;

			for (int idx = 0; idx < text.Length; idx++)
			{
				if (text[idx] == '/')
				{
					if (lastSlashIdx == idx - 1)
					{
						throw new ArgumentException("Leading and consecutive slashes are not permitted in blob identifiers", name);
					}
					else
					{
						lastSlashIdx = idx;
					}
				}
				else
				{
					if (!IsValidChar(text[idx]))
					{
						throw new ArgumentException($"'{(char)text[idx]} is not a valid blob identifier character", name);
					}
				}
			}
		}

		/// <summary>
		/// Sanitize the given string to make a valid blob id
		/// </summary>
		/// <param name="name"></param>
		/// <returns></returns>
		public static Utf8String Sanitize(Utf8String name)
		{
			byte[] output = new byte[name.Length];

			int outputIdx = 0;
			for (int idx = 0; idx < name.Length; idx++)
			{
				if (name[idx] >= 'A' && name[idx] <= 'Z')
				{
					output[outputIdx++] = (byte)(name[idx] + 'a' - 'A');
				}
				else if ((name[idx] >= 'a' && name[idx] <= 'z') || (name[idx] >= '0' && name[idx] <= '9') || name[idx] == '+')
				{
					output[outputIdx++] = name[idx];
				}
				else if (name[idx] == '/' && outputIdx > 0)
				{
					output[outputIdx++] = (byte)'/';
				}
				else if (name.Length > 0 && name[name.Length - 1] != '-')
				{
					output[outputIdx++] = (byte)'-';
				}
			}

			while (outputIdx > 0 && (output[outputIdx - 1] == '-' || output[outputIdx - 1] == '/'))
			{
				outputIdx--;
			}

			return new Utf8String(output.AsMemory(0, outputIdx));
		}

		/// <summary>
		/// Checks whether this blob is within the given folder
		/// </summary>
		/// <param name="folderName">Name of the folder</param>
		/// <returns>True if the the blob id is within the given folder</returns>
		public bool WithinFolder(Utf8String folderName)
		{
			return Inner.Length > folderName.Length && Inner.StartsWith(folderName) && Inner[folderName.Length] == '/';
		}

		/// <summary>
		/// Test if a given character is valid in a store id
		/// </summary>
		/// <param name="c">Character to test</param>
		/// <returns>True if the character is valid</returns>
		static bool IsValidChar(byte c) => (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '_' || c == '-');

		/// <inheritdoc/>
		public override bool Equals(object? obj) => obj is BlobId BlobId && Equals(BlobId);

		/// <inheritdoc/>
		public override int GetHashCode() => Inner.GetHashCode();

		/// <inheritdoc/>
		public bool Equals(BlobId BlobId) => Inner == BlobId.Inner;

		/// <inheritdoc/>
		public override string ToString() => Inner.ToString();

		/// <inheritdoc/>
		public static bool operator ==(BlobId lhs, BlobId rhs) => lhs.Equals(rhs);

		/// <inheritdoc/>
		public static bool operator !=(BlobId lhs, BlobId rhs) => !lhs.Equals(rhs);
	}

	/// <summary>
	/// Type converter for <see cref="BlobId"/> to and from JSON
	/// </summary>
	sealed class BlobIdJsonConverter : JsonConverter<BlobId>
	{
		/// <inheritdoc/>
		public override BlobId Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options) => new BlobId(new Utf8String(reader.GetUtf8String().ToArray()));

		/// <inheritdoc/>
		public override void Write(Utf8JsonWriter writer, BlobId value, JsonSerializerOptions options) => writer.WriteStringValue(value.Inner.Span);
	}

	/// <summary>
	/// Type converter from strings to <see cref="BlobId"/> objects
	/// </summary>
	sealed class BlobIdTypeConverter : TypeConverter
	{
		/// <inheritdoc/>
		public override bool CanConvertFrom(ITypeDescriptorContext? context, Type sourceType)
		{
			return sourceType == typeof(string);
		}

		/// <inheritdoc/>
		public override object ConvertFrom(ITypeDescriptorContext? context, CultureInfo? culture, object? value)
		{
			return new BlobId((string)value!);
		}
	}

	/// <summary>
	/// Type converter to compact binary
	/// </summary>
	sealed class BlobIdCbConverter : CbConverterBase<BlobId>
	{
		/// <inheritdoc/>
		public override BlobId Read(CbField field) => new BlobId(field.AsUtf8String());

		/// <inheritdoc/>
		public override void Write(CbWriter writer, BlobId value) => writer.WriteUtf8StringValue(value.Inner);

		/// <inheritdoc/>
		public override void WriteNamed(CbWriter writer, Utf8String name, BlobId value) => writer.WriteUtf8String(name, value.Inner);
	}

	/// <summary>
	/// Extension methods for blob ids
	/// </summary>
	public static class BlobIdExtensions
	{
		/// <summary>
		/// Deserialize a blob id
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>The blob id that was read</returns>
		public static BlobId ReadBlobId(this IMemoryReader reader)
		{
			return new BlobId(reader.ReadUtf8String());
		}

		/// <summary>
		/// Serialize a blob id
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="value">Value to serialize</param>
		public static void WriteBlobId(this IMemoryWriter writer, BlobId value)
		{
			writer.WriteUtf8String(value.Inner);
		}
	}
}
