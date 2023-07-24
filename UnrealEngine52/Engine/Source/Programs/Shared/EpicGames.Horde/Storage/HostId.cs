// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Serialization;
using System;
using System.ComponentModel;
using System.Globalization;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Identifies a host in the storage hierarchy, via a string of identifiers separated with colons.
	/// </summary>
	[JsonConverter(typeof(HostIdJsonConverter))]
	[TypeConverter(typeof(HostIdTypeConverter))]
	[CbConverter(typeof(HostIdCbConverter))]
	public struct HostId : IEquatable<HostId>
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
		/// Empty host id
		/// </summary>
		public static HostId Empty { get; } = default;

		/// <summary>
		/// Identifier for the host
		/// </summary>
		public Utf8String Inner { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="inner"></param>
		public HostId(Utf8String inner)
		{
			Inner = inner;
			ValidateArgument(nameof(inner), inner);
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="inner"></param>
		/// <param name="sanitize"></param>
		public HostId(Utf8String inner, Sanitize sanitize)
		{
			Inner = inner;
			_ = sanitize;
		}

		/// <summary>
		/// Appends another identifier to this host id
		/// </summary>
		/// <param name="identifier"></param>
		/// <returns></returns>
		public HostId Append(Utf8String identifier)
		{
			ValidateArgument(nameof(identifier), identifier);

			byte[] buffer = new byte[Inner.Length + 1 + identifier.Length];
			Inner.Span.CopyTo(buffer);
			buffer[Inner.Length] = (byte)':';
			identifier.Span.CopyTo(buffer.AsSpan(Inner.Length + 1));

			return new HostId(new Utf8String(buffer), Sanitize.None);
		}

		/// <summary>
		/// Validates a given string as a host id
		/// </summary>
		/// <param name="name">Name of the argument</param>
		/// <param name="text">String to validate</param>
		public static void ValidateArgument(string name, Utf8String text)
		{
			if (text.Length == 0)
			{
				return;
			}
			if (text[^1] == ':')
			{
				throw new ArgumentException("Host identifiers cannot end with a colon", name);
			}

			int lastColonIdx = -1;

			for (int idx = 0; idx < text.Length; idx++)
			{
				if (text[idx] == ':')
				{
					if (lastColonIdx == idx - 1)
					{
						throw new ArgumentException("Leading and consecutive colons are not permitted in host identifiers", name);
					}
					else
					{
						lastColonIdx = idx;
					}
				}
				else
				{
					if (!IsValidChar(text[idx]))
					{
						throw new ArgumentException($"'{(char)text[idx]}' is not a valid host identifier character", name);
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

		/// <summary>
		/// Checks whether this blob id is valid
		/// </summary>
		/// <returns>True if the identifier is valid</returns>
		public bool IsValid() => Inner.Length > 0;

		/// <summary>
		/// Gets the upstream host id
		/// </summary>
		/// <returns>The upstream host id</returns>
		public HostId GetUpstream()
		{
			int idx = Inner.LastIndexOf(':');
			return (idx == -1) ? Empty : new HostId(Inner.Substring(0, idx), Sanitize.None);
		}

		/// <inheritdoc/>
		public override bool Equals(object? obj) => obj is HostId HostId && Equals(HostId);

		/// <inheritdoc/>
		public override int GetHashCode() => Inner.GetHashCode();

		/// <inheritdoc/>
		public bool Equals(HostId HostId) => Inner == HostId.Inner;

		/// <inheritdoc/>
		public override string ToString() => Inner.ToString();

		/// <inheritdoc/>
		public static bool operator ==(HostId lhs, HostId rhs) => lhs.Equals(rhs);

		/// <inheritdoc/>
		public static bool operator !=(HostId lhs, HostId rhs) => !lhs.Equals(rhs);
	}

	/// <summary>
	/// Type converter for <see cref="HostId"/> to and from JSON
	/// </summary>
	sealed class HostIdJsonConverter : JsonConverter<HostId>
	{
		/// <inheritdoc/>
		public override HostId Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options) => new HostId(new Utf8String(reader.GetUtf8String().ToArray()));

		/// <inheritdoc/>
		public override void Write(Utf8JsonWriter writer, HostId value, JsonSerializerOptions options) => writer.WriteStringValue(value.Inner.Span);
	}

	/// <summary>
	/// Type converter from strings to <see cref="HostId"/> objects
	/// </summary>
	sealed class HostIdTypeConverter : TypeConverter
	{
		/// <inheritdoc/>
		public override bool CanConvertFrom(ITypeDescriptorContext? context, Type sourceType)
		{
			return sourceType == typeof(string);
		}

		/// <inheritdoc/>
		public override object ConvertFrom(ITypeDescriptorContext? context, CultureInfo? culture, object? value)
		{
			return new HostId((string)value!);
		}
	}

	/// <summary>
	/// Type converter to compact binary
	/// </summary>
	sealed class HostIdCbConverter : CbConverterBase<HostId>
	{
		/// <inheritdoc/>
		public override HostId Read(CbField field) => new HostId(field.AsUtf8String());

		/// <inheritdoc/>
		public override void Write(CbWriter writer, HostId value) => writer.WriteUtf8StringValue(value.Inner);

		/// <inheritdoc/>
		public override void WriteNamed(CbWriter writer, Utf8String name, HostId value) => writer.WriteUtf8String(name, value.Inner);
	}

	/// <summary>
	/// Extension methods for <see cref="HostId"/>s.
	/// </summary>
	public static class HostIdExtensions
	{
		/// <summary>
		/// Deserialize a <see cref="HostId"/>
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>The blob id that was read</returns>
		public static HostId ReadHostId(this IMemoryReader reader)
		{
			return new HostId(reader.ReadUtf8String());
		}

		/// <summary>
		/// Serialize a <see cref="HostId"/>
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="value">Value to serialize</param>
		public static void WriteHostId(this IMemoryWriter writer, HostId value)
		{
			writer.WriteUtf8String(value.Inner);
		}
	}
}
