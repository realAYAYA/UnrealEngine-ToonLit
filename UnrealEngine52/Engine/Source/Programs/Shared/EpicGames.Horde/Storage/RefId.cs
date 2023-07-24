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
	/// Identifier for a storage namespace
	/// </summary>
	[JsonConverter(typeof(RefIdJsonConverter))]
	[TypeConverter(typeof(RefIdTypeConverter))]
	[CbConverter(typeof(RefIdCbConverter))]
	public struct RefId : IEquatable<RefId>
	{
		/// <summary>
		/// Hash identifier for the ref
		/// </summary>
		public IoHash Hash { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="hash"></param>
		public RefId(IoHash hash)
		{
			Hash = hash;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="name">Name to identify this ref</param>
		public RefId(string name)
		{
			Hash = IoHash.Compute(Encoding.UTF8.GetBytes(name));
		}

		/// <inheritdoc/>
		public override bool Equals(object? obj) => obj is RefId refId && Equals(refId);

		/// <inheritdoc/>
		public override int GetHashCode() => Hash.GetHashCode();

		/// <inheritdoc/>
		public bool Equals(RefId refId) => Hash == refId.Hash;

		/// <inheritdoc/>
		public override string ToString() => Hash.ToString();

		/// <inheritdoc/>
		public static bool operator ==(RefId lhs, RefId rhs) => lhs.Equals(rhs);

		/// <inheritdoc/>
		public static bool operator !=(RefId lhs, RefId rhs) => !lhs.Equals(rhs);
	}

	/// <summary>
	/// Type converter for IoHash to and from JSON
	/// </summary>
	sealed class RefIdJsonConverter : JsonConverter<RefId>
	{
		/// <inheritdoc/>
		public override RefId Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options) => new RefId(IoHash.Parse(reader.ValueSpan));

		/// <inheritdoc/>
		public override void Write(Utf8JsonWriter writer, RefId value, JsonSerializerOptions options) => writer.WriteStringValue(value.Hash.ToUtf8String().Span);
	}

	/// <summary>
	/// Type converter from strings to IoHash objects
	/// </summary>
	sealed class RefIdTypeConverter : TypeConverter
	{
		/// <inheritdoc/>
		public override bool CanConvertFrom(ITypeDescriptorContext? context, Type sourceType)
		{
			return sourceType == typeof(string);
		}

		/// <inheritdoc/>
		public override object ConvertFrom(ITypeDescriptorContext? context, CultureInfo? culture, object? value)
		{
			IoHash hash;
			if (IoHash.TryParse((string)value!, out hash))
			{
				return new RefId(hash);
			}
			else
			{
				return new RefId((string)value!);
			}
		}
	}

	/// <summary>
	/// Type converter to compact binary
	/// </summary>
	sealed class RefIdCbConverter : CbConverterBase<RefId>
	{
		/// <inheritdoc/>
		public override RefId Read(CbField field) => new RefId(field.AsHash());

		/// <inheritdoc/>
		public override void Write(CbWriter writer, RefId value) => writer.WriteHashValue(value.Hash);

		/// <inheritdoc/>
		public override void WriteNamed(CbWriter writer, Utf8String name, RefId value) => writer.WriteHash(name, value.Hash);
	}
}
