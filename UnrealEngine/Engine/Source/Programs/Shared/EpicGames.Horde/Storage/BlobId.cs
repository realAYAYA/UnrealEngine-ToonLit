// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Serialization;
using System;
using System.ComponentModel;
using System.Globalization;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Unique identifier for a blob, as a utf-8 string. Clients should not assume any internal structure to this identifier; it only
	/// has meaning to the <see cref="IBlobStore"/> implementation.
	/// </summary>
	[JsonConverter(typeof(BlobIdJsonConverter))]
	[TypeConverter(typeof(BlobIdTypeConverter))]
	[CbConverter(typeof(BlobIdCbConverter))]
	public struct BlobId : IEquatable<BlobId>
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
		/// <param name="sanitize"></param>
		public BlobId(Utf8String inner, Sanitize sanitize)
		{
			Inner = inner;
			_ = sanitize;
		}

		/// <summary>
		/// Create a unique content id, optionally including a ref name
		/// </summary>
		/// <param name="serverId">The server id</param>
		/// <param name="prefix">Prefix for blob names. Follows the same restrictions as for content ids.</param>
		/// <returns>New content id</returns>
		public static BlobId Create(ServerId serverId, Utf8String prefix = default)
		{
			if (prefix.Length == 0)
			{
				DateTime now = DateTime.UtcNow.Date;
				prefix = $"_by_date_/{now.Year}-{now.Month:D2}/{now.Day:D2}";
			}
			else
			{
				ContentId.ValidateArgument(nameof(prefix), prefix);
			}

			byte[] buffer;
			Span<byte> span;

			if (serverId.IsValid())
			{
				buffer = new byte[serverId.Inner.Length + 1 + prefix.Length + 1 + 24];
				span = buffer;

				serverId.Inner.Span.CopyTo(span);
				span = span.Slice(serverId.Inner.Length);

				span[0] = (byte)':';
				span = span.Slice(1);
			}
			else
			{
				buffer = new byte[prefix.Length + 1 + 24];
				span = buffer;
			}

			prefix.Span.CopyTo(span);
			span = span.Slice(prefix.Length);

			span[0] = (byte)'/';
			span = span.Slice(1);

			ContentId.GenerateUniqueId(span);
			return new BlobId(new Utf8String(buffer), Sanitize.None);
		}

		/// <summary>
		/// Validates a given string as a content id
		/// </summary>
		/// <param name="name">Name of the argument</param>
		/// <param name="text">String to validate</param>
		public static void ValidateArgument(string name, Utf8String text)
		{
			if (text.Length == 0)
			{
				throw new ArgumentException("Blob identifiers cannot be empty", name);
			}

			int colonIdx = text.LastIndexOf((byte)':');
			if (colonIdx != -1)
			{
				ServerId.ValidateArgument(name, text.Substring(0, colonIdx));
			}

			ContentId.ValidateArgument(name, text.Substring(colonIdx + 1));
		}

		/// <summary>
		/// Checks whether this blob id is valid
		/// </summary>
		/// <returns>True if the identifier is valid</returns>
		public bool IsValid() => Inner.Length > 0;

		/// <summary>
		/// Gets the server id for this blob
		/// </summary>
		/// <returns>Server id</returns>
		public ServerId GetServerId()
		{
			int colonIdx = Inner.LastIndexOf((byte)':');
			if (colonIdx == -1)
			{
				return ServerId.Empty;
			}
			else
			{
				return new ServerId(Inner.Substring(0, colonIdx), ServerId.Sanitize.None);
			}
		}

		/// <summary>
		/// Gets the content id for this blob
		/// </summary>
		/// <returns>Content id</returns>
		public ContentId GetContentId()
		{
			int colonIdx = Inner.LastIndexOf((byte)':');
			return new ContentId(Inner.Substring(colonIdx + 1), ContentId.Sanitize.None);
		}

		/// <inheritdoc/>
		public override bool Equals(object? obj) => obj is BlobId blobId && Equals(blobId);

		/// <inheritdoc/>
		public override int GetHashCode() => Inner.GetHashCode();

		/// <inheritdoc/>
		public bool Equals(BlobId blobId) => Inner == blobId.Inner;

		/// <inheritdoc/>
		public override string ToString() => Inner.ToString();

		/// <inheritdoc/>
		public static bool operator ==(BlobId lhs, BlobId rhs) => lhs.Equals(rhs);

		/// <inheritdoc/>
		public static bool operator !=(BlobId lhs, BlobId rhs) => !lhs.Equals(rhs);
	}

	/// <summary>
	/// Type converter for BlobId to and from JSON
	/// </summary>
	sealed class BlobIdJsonConverter : JsonConverter<BlobId>
	{
		/// <inheritdoc/>
		public override BlobId Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options) => new BlobId(new Utf8String(reader.GetUtf8String().ToArray()));

		/// <inheritdoc/>
		public override void Write(Utf8JsonWriter writer, BlobId value, JsonSerializerOptions options) => writer.WriteStringValue(value.Inner.Span);
	}

	/// <summary>
	/// Type converter from strings to BlobId objects
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
