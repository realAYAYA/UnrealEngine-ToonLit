// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Serialization;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using System;
using System.ComponentModel;
using System.Globalization;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Unique identifier for a blob, as a utf-8 string. Clients should not assume any internal structure to this identifier; it only
	/// has meaning to the <see cref="IStorageClient"/> implementation.
	/// </summary>
	[JsonConverter(typeof(BlobLocatorJsonConverter))]
	[TypeConverter(typeof(BlobLocatorTypeConverter))]
	[CbConverter(typeof(BlobLocatorCbConverter))]
	public struct BlobLocator : IEquatable<BlobLocator>
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
		/// Empty blob locator
		/// </summary>
		public static BlobLocator Empty { get; } = default;

		/// <summary>
		/// Identifier for the blob
		/// </summary>
		public Utf8String Inner { get; }

		/// <summary>
		/// Gets the server id for this blob
		/// </summary>
		/// <returns>Server id</returns>
		public HostId HostId
		{
			get
			{
				int colonIdx = Inner.LastIndexOf((byte)':');
				return (colonIdx == -1) ? HostId.Empty : new HostId(Inner.Substring(0, colonIdx), HostId.Sanitize.None);
			}
		}

		/// <summary>
		/// Gets the content id for this blob
		/// </summary>
		/// <returns>Content id</returns>
		public BlobId BlobId
		{
			get
			{
				int colonIdx = Inner.LastIndexOf((byte)':');
				return new BlobId(Inner.Substring(colonIdx + 1), BlobId.Validate.None);
			}
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="inner"></param>
		public BlobLocator(Utf8String inner)
		{
			if (inner.Length == 0)
			{
				Inner = default;
			}
			else
			{
				Inner = inner;
				ValidateArgument(nameof(inner), inner);
			}
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="hostId"></param>
		/// <param name="blobId"></param>
		public BlobLocator(HostId hostId, BlobId blobId)
		{
			if (hostId.IsValid())
			{
				byte[] buffer = new byte[hostId.Inner.Length + 1 + blobId.Inner.Length];

				hostId.Inner.Span.CopyTo(buffer);
				buffer[hostId.Inner.Length] = (byte)':';
				blobId.Inner.Span.CopyTo(buffer.AsSpan(hostId.Inner.Length + 1));

				Inner = new Utf8String(buffer);
			}
			else
			{
				Inner = blobId.Inner;
			}
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="hostId"></param>
		/// <param name="blobId"></param>
		public BlobLocator(ReadOnlySpan<byte> hostId, ReadOnlySpan<byte> blobId)
		{
			if (hostId.Length > 0)
			{
				byte[] buffer = new byte[hostId.Length + 1 + blobId.Length];

				hostId.CopyTo(buffer);
				buffer[hostId.Length] = (byte)':';
				blobId.CopyTo(buffer.AsSpan(hostId.Length + 1));

				Inner = new Utf8String(buffer);
			}
			else
			{
				Inner = new Utf8String(blobId.ToArray());
			}
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="inner"></param>
		/// <param name="sanitize"></param>
		public BlobLocator(Utf8String inner, Sanitize sanitize)
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
		public static BlobLocator Create(HostId serverId, Utf8String prefix = default)
		{
			int length = 24;
			if (serverId.IsValid())
			{
				length += serverId.Inner.Length + 1;
			}
			if (prefix.Length > 0)
			{
				length += prefix.Length + 1;
			}

			byte[] buffer = new byte[length];
			Span<byte> span = buffer;

			if (serverId.IsValid())
			{
				serverId.Inner.Span.CopyTo(span);
				span = span.Slice(serverId.Inner.Length);

				span[0] = (byte)':';
				span = span.Slice(1);
			}
			if (prefix.Length > 0)
			{
				BlobId.ValidateArgument(nameof(prefix), prefix);

				prefix.Span.CopyTo(span);
				span = span.Slice(prefix.Length);

				span[0] = (byte)'/';
				span = span.Slice(1);
			}

			BlobId.GenerateUniqueId(span);
			return new BlobLocator(new Utf8String(buffer), Sanitize.None);
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
				HostId.ValidateArgument(name, text.Substring(0, colonIdx));
			}

			BlobId.ValidateArgument(name, text.Substring(colonIdx + 1));
		}

		/// <summary>
		/// Checks whether this blob id is valid
		/// </summary>
		/// <returns>True if the identifier is valid</returns>
		public bool IsValid() => Inner.Length > 0;

		/// <inheritdoc/>
		public override bool Equals(object? obj) => obj is BlobLocator blobId && Equals(blobId);

		/// <inheritdoc/>
		public override int GetHashCode() => Inner.GetHashCode();

		/// <inheritdoc/>
		public bool Equals(BlobLocator locator) => Inner == locator.Inner;

		/// <inheritdoc/>
		public override string ToString() => Inner.ToString();

		/// <inheritdoc/>
		public static bool operator ==(BlobLocator lhs, BlobLocator rhs) => lhs.Equals(rhs);

		/// <inheritdoc/>
		public static bool operator !=(BlobLocator lhs, BlobLocator rhs) => !lhs.Equals(rhs);
	}

	/// <summary>
	/// Type converter for BlobId to and from JSON
	/// </summary>
	sealed class BlobLocatorJsonConverter : JsonConverter<BlobLocator>
	{
		/// <inheritdoc/>
		public override BlobLocator Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options) => new BlobLocator(new Utf8String(reader.GetUtf8String().ToArray()));

		/// <inheritdoc/>
		public override void Write(Utf8JsonWriter writer, BlobLocator value, JsonSerializerOptions options) => writer.WriteStringValue(value.Inner.Span);
	}

	/// <summary>
	/// Type converter from strings to BlobId objects
	/// </summary>
	sealed class BlobLocatorTypeConverter : TypeConverter
	{
		/// <inheritdoc/>
		public override bool CanConvertFrom(ITypeDescriptorContext? context, Type sourceType)
		{
			return sourceType == typeof(string);
		}

		/// <inheritdoc/>
		public override object ConvertFrom(ITypeDescriptorContext? context, CultureInfo? culture, object? value)
		{
			return new BlobLocator((string)value!);
		}
	}

	/// <summary>
	/// Type converter to compact binary
	/// </summary>
	sealed class BlobLocatorCbConverter : CbConverterBase<BlobLocator>
	{
		/// <inheritdoc/>
		public override BlobLocator Read(CbField field) => new BlobLocator(field.AsUtf8String());

		/// <inheritdoc/>
		public override void Write(CbWriter writer, BlobLocator value) => writer.WriteUtf8StringValue(value.Inner);

		/// <inheritdoc/>
		public override void WriteNamed(CbWriter writer, Utf8String name, BlobLocator value) => writer.WriteUtf8String(name, value.Inner);
	}

	/// <summary>
	/// Extension methods for blob locators
	/// </summary>
	public static class BlobLocatorExtensions
	{
		/// <summary>
		/// Deserialize a blob locator
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>The blob id that was read</returns>
		public static BlobLocator ReadBlobLocator(this IMemoryReader reader)
		{
			return new BlobLocator(reader.ReadUtf8String());
		}

		/// <summary>
		/// Serialize a blob locator
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="value">Value to serialize</param>
		public static void WriteBlobLocator(this IMemoryWriter writer, BlobLocator value)
		{
			writer.WriteUtf8String(value.Inner);
		}
	}
}
