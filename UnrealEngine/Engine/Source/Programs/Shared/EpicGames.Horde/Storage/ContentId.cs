// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Serialization;
using System;
using System.Buffers.Binary;
using System.ComponentModel;
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
	[JsonConverter(typeof(ContentIdJsonConverter))]
	[TypeConverter(typeof(ContentIdTypeConverter))]
	[CbConverter(typeof(ContentIdCbConverter))]
	public struct ContentId : IEquatable<ContentId>
	{
		/// <summary>
		/// Dummy enum to allow invoking the constructor which takes a sanitized full path
		/// </summary>
		public enum Sanitize
		{
			/// <summary>
			/// Dummy value
			/// </summary>
			None
		}

		/// <summary>
		/// Empty blob id
		/// </summary>
		public static ContentId Empty { get; } = default;

		/// <summary>
		/// Identifier for the ref
		/// </summary>
		public Utf8String Inner { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="inner"></param>
		public ContentId(Utf8String inner)
		{
			Inner = inner;
			ValidateArgument(nameof(inner), inner);
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="inner"></param>
		/// <param name="sanitize"></param>
		public ContentId(Utf8String inner, Sanitize sanitize)
		{
			Inner = inner;
			_ = sanitize;
		}

		static readonly Utf8String s_process;
		static int s_counter;

		/// <summary>
		/// Static constructor
		/// </summary>
		[System.Diagnostics.CodeAnalysis.SuppressMessage("Usage", "CA2207:Initialize value type static fields inline")]
		static ContentId()
		{
			Random rnd = new Random(HashCode.Combine(DateTime.UtcNow.Ticks, Environment.ProcessId, Environment.TickCount64, Environment.MachineName.GetHashCode(StringComparison.Ordinal)));

			Span<byte> process = stackalloc byte[4];
			rnd.NextBytes(process);
			s_process = StringUtils.FormatUtf8HexString(process);

			s_counter = rnd.Next();
		}

		/// <summary>
		/// Creates a new identifier in the given buffer.
		/// </summary>
		/// <param name="output">Buffer to receive the output. The first 24 bytes will be written to.</param>
		public static void GenerateUniqueId(Span<byte> output)
		{
			uint timestamp = (uint)((DateTime.UtcNow - DateTime.UnixEpoch).Ticks / TimeSpan.TicksPerSecond);
			StringUtils.FormatUtf8HexString(timestamp, output);

			s_process.Span.CopyTo(output.Slice(8));

			uint counter = (uint)Interlocked.Increment(ref s_counter);
			StringUtils.FormatUtf8HexString(counter, output.Slice(16));
		}

		/// <summary>
		/// Checks whether this blob id is valid
		/// </summary>
		/// <returns>True if the identifier is valid</returns>
		public bool IsValid() => Inner.Length > 0;

		/// <summary>
		/// Validates a given string as a content id
		/// </summary>
		/// <param name="name">Name of the argument</param>
		/// <param name="text">String to validate</param>
		public static void ValidateArgument(string name, Utf8String text)
		{
			if (text.Length == 0)
			{
				throw new ArgumentException("Content identifiers cannot be empty", name);
			}
			if (text[^1] == '/')
			{
				throw new ArgumentException("Content identifiers cannot start or end with a slash", name);
			}

			int lastSlashIdx = -1;

			for (int idx = 0; idx < text.Length; idx++)
			{
				if (text[idx] == '/')
				{
					if (lastSlashIdx == idx - 1)
					{
						throw new ArgumentException("Leading and consecutive slashes are not permitted in content identifiers", name);
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
						throw new ArgumentException($"'{(char)text[idx]} is not a valid content identifier character", name);
					}
				}
			}
		}

		/// <summary>
		/// Test if a given character is valid in a store id
		/// </summary>
		/// <param name="c">Character to test</param>
		/// <returns>True if the character is valid</returns>
		static bool IsValidChar(byte c) => (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '_' || c == '-');

		/// <inheritdoc/>
		public override bool Equals(object? obj) => obj is ContentId ContentId && Equals(ContentId);

		/// <inheritdoc/>
		public override int GetHashCode() => Inner.GetHashCode();

		/// <inheritdoc/>
		public bool Equals(ContentId ContentId) => Inner == ContentId.Inner;

		/// <inheritdoc/>
		public override string ToString() => Inner.ToString();

		/// <inheritdoc/>
		public static bool operator ==(ContentId lhs, ContentId rhs) => lhs.Equals(rhs);

		/// <inheritdoc/>
		public static bool operator !=(ContentId lhs, ContentId rhs) => !lhs.Equals(rhs);
	}

	/// <summary>
	/// Type converter for ContentId to and from JSON
	/// </summary>
	sealed class ContentIdJsonConverter : JsonConverter<ContentId>
	{
		/// <inheritdoc/>
		public override ContentId Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options) => new ContentId(new Utf8String(reader.GetUtf8String().ToArray()));

		/// <inheritdoc/>
		public override void Write(Utf8JsonWriter writer, ContentId value, JsonSerializerOptions options) => writer.WriteStringValue(value.Inner.Span);
	}

	/// <summary>
	/// Type converter from strings to ContentId objects
	/// </summary>
	sealed class ContentIdTypeConverter : TypeConverter
	{
		/// <inheritdoc/>
		public override bool CanConvertFrom(ITypeDescriptorContext? context, Type sourceType)
		{
			return sourceType == typeof(string);
		}

		/// <inheritdoc/>
		public override object ConvertFrom(ITypeDescriptorContext? context, CultureInfo? culture, object? value)
		{
			return new ContentId((string)value!);
		}
	}

	/// <summary>
	/// Type converter to compact binary
	/// </summary>
	sealed class ContentIdCbConverter : CbConverterBase<ContentId>
	{
		/// <inheritdoc/>
		public override ContentId Read(CbField field) => new ContentId(field.AsUtf8String());

		/// <inheritdoc/>
		public override void Write(CbWriter writer, ContentId value) => writer.WriteUtf8StringValue(value.Inner);

		/// <inheritdoc/>
		public override void WriteNamed(CbWriter writer, Utf8String name, ContentId value) => writer.WriteUtf8String(name, value.Inner);
	}

	/// <summary>
	/// Extension methods for blob ids
	/// </summary>
	public static class ContentIdExtensions
	{
		/// <summary>
		/// Deserialize a blob id
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>The blob id that was read</returns>
		public static ContentId ReadContentId(this IMemoryReader reader)
		{
			return new ContentId(reader.ReadUtf8String());
		}

		/// <summary>
		/// Serialize a blob id
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="value">Value to serialize</param>
		public static void WriteContentId(this IMemoryWriter writer, ContentId value)
		{
			writer.WriteUtf8String(value.Inner);
		}
	}
}
