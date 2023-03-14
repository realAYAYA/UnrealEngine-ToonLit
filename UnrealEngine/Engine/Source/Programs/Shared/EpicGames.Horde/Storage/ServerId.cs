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
	/// Identifies a server in the storage hierarchy, via a string of identifiers separated with colons.
	/// </summary>
	[JsonConverter(typeof(ServerIdJsonConverter))]
	[TypeConverter(typeof(ServerIdTypeConverter))]
	[CbConverter(typeof(ServerIdCbConverter))]
	public struct ServerId : IEquatable<ServerId>
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
		/// Empty server id
		/// </summary>
		public static ServerId Empty { get; } = default;

		/// <summary>
		/// Identifier for the ref
		/// </summary>
		public Utf8String Inner { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="inner"></param>
		public ServerId(Utf8String inner)
		{
			Inner = inner;
			ValidateArgument(nameof(inner), inner);
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="inner"></param>
		/// <param name="sanitize"></param>
		public ServerId(Utf8String inner, Sanitize sanitize)
		{
			Inner = inner;
			_ = sanitize;
		}

		/// <summary>
		/// Appends another identifier to this server id
		/// </summary>
		/// <param name="identifier"></param>
		/// <returns></returns>
		public ServerId Append(Utf8String identifier)
		{
			ValidateArgument(nameof(identifier), identifier);

			byte[] buffer = new byte[Inner.Length + 1 + identifier.Length];
			Inner.Span.CopyTo(buffer);
			buffer[Inner.Length] = (byte)':';
			identifier.Span.CopyTo(buffer.AsSpan(Inner.Length + 1));

			return new ServerId(new Utf8String(buffer), Sanitize.None);
		}

		/// <summary>
		/// Validates a given string as a server id
		/// </summary>
		/// <param name="name">Name of the argument</param>
		/// <param name="text">String to validate</param>
		public static void ValidateArgument(string name, Utf8String text)
		{
			if (text.Length == 0)
			{
				throw new ArgumentException("Server identifiers cannot be empty", name);
			}
			if (text[^1] == ':')
			{
				throw new ArgumentException("Server identifiers cannot end with a colon", name);
			}

			int lastColonIdx = -1;

			for (int idx = 0; idx < text.Length; idx++)
			{
				if (text[idx] == ':')
				{
					if (lastColonIdx == idx - 1)
					{
						throw new ArgumentException("Leading and consecutive colons are not permitted in server identifiers", name);
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
						throw new ArgumentException($"'{(char)text[idx]} is not a valid server identifier character", name);
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
		/// Gets the upstream server id
		/// </summary>
		/// <returns>The upstream server id</returns>
		public ServerId GetUpstream()
		{
			int idx = Inner.LastIndexOf(':');
			return (idx == -1) ? Empty : new ServerId(Inner.Substring(0, idx), Sanitize.None);
		}

		/// <inheritdoc/>
		public override bool Equals(object? obj) => obj is ServerId ServerId && Equals(ServerId);

		/// <inheritdoc/>
		public override int GetHashCode() => Inner.GetHashCode();

		/// <inheritdoc/>
		public bool Equals(ServerId ServerId) => Inner == ServerId.Inner;

		/// <inheritdoc/>
		public override string ToString() => Inner.ToString();

		/// <inheritdoc/>
		public static bool operator ==(ServerId lhs, ServerId rhs) => lhs.Equals(rhs);

		/// <inheritdoc/>
		public static bool operator !=(ServerId lhs, ServerId rhs) => !lhs.Equals(rhs);
	}

	/// <summary>
	/// Type converter for ServerId to and from JSON
	/// </summary>
	sealed class ServerIdJsonConverter : JsonConverter<ServerId>
	{
		/// <inheritdoc/>
		public override ServerId Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options) => new ServerId(new Utf8String(reader.GetUtf8String().ToArray()));

		/// <inheritdoc/>
		public override void Write(Utf8JsonWriter writer, ServerId value, JsonSerializerOptions options) => writer.WriteStringValue(value.Inner.Span);
	}

	/// <summary>
	/// Type converter from strings to ServerId objects
	/// </summary>
	sealed class ServerIdTypeConverter : TypeConverter
	{
		/// <inheritdoc/>
		public override bool CanConvertFrom(ITypeDescriptorContext? context, Type sourceType)
		{
			return sourceType == typeof(string);
		}

		/// <inheritdoc/>
		public override object ConvertFrom(ITypeDescriptorContext? context, CultureInfo? culture, object? value)
		{
			return new ServerId((string)value!);
		}
	}

	/// <summary>
	/// Type converter to compact binary
	/// </summary>
	sealed class ServerIdCbConverter : CbConverterBase<ServerId>
	{
		/// <inheritdoc/>
		public override ServerId Read(CbField field) => new ServerId(field.AsUtf8String());

		/// <inheritdoc/>
		public override void Write(CbWriter writer, ServerId value) => writer.WriteUtf8StringValue(value.Inner);

		/// <inheritdoc/>
		public override void WriteNamed(CbWriter writer, Utf8String name, ServerId value) => writer.WriteUtf8String(name, value.Inner);
	}

	/// <summary>
	/// Extension methods for blob ids
	/// </summary>
	public static class ServerIdExtensions
	{
		/// <summary>
		/// Deserialize a blob id
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>The blob id that was read</returns>
		public static ServerId ReadServerId(this IMemoryReader reader)
		{
			return new ServerId(reader.ReadUtf8String());
		}

		/// <summary>
		/// Serialize a blob id
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="value">Value to serialize</param>
		public static void WriteServerId(this IMemoryWriter writer, ServerId value)
		{
			writer.WriteUtf8String(value.Inner);
		}
	}
}
