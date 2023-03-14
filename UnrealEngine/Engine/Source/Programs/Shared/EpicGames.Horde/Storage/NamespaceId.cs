// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel;
using System.Globalization;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Identifier for a storage namespace
	/// </summary>
	[JsonConverter(typeof(NamespaceIdJsonConverter))]
	[TypeConverter(typeof(NamespaceIdTypeConverter))]
	public struct NamespaceId : IEquatable<NamespaceId>
	{
		/// <summary>
		/// The text representing this id
		/// </summary>
		readonly StringId _inner;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="input">Unique id for the string</param>
		public NamespaceId(string input)
		{
			_inner = new StringId(input);
		}

		/// <inheritdoc/>
		public override bool Equals(object? obj) => obj is NamespaceId id && _inner.Equals(id._inner);

		/// <inheritdoc/>
		public override int GetHashCode() => _inner.GetHashCode();

		/// <inheritdoc/>
		public bool Equals(NamespaceId other) => _inner.Equals(other._inner);

		/// <inheritdoc/>
		public override string ToString() => _inner.ToString();

		/// <inheritdoc cref="StringId.op_Equality"/>
		public static bool operator ==(NamespaceId left, NamespaceId right) => left._inner == right._inner;

		/// <inheritdoc cref="StringId.op_Inequality"/>
		public static bool operator !=(NamespaceId left, NamespaceId right) => left._inner != right._inner;
	}

	/// <summary>
	/// Type converter for NamespaceId to and from JSON
	/// </summary>
	sealed class NamespaceIdJsonConverter : JsonConverter<NamespaceId>
	{
		/// <inheritdoc/>
		public override NamespaceId Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options) => new NamespaceId(reader.GetString() ?? String.Empty);

		/// <inheritdoc/>
		public override void Write(Utf8JsonWriter writer, NamespaceId value, JsonSerializerOptions options) => writer.WriteStringValue(value.ToString());
	}

	/// <summary>
	/// Type converter from strings to NamespaceId objects
	/// </summary>
	sealed class NamespaceIdTypeConverter : TypeConverter
	{
		/// <inheritdoc/>
		public override bool CanConvertFrom(ITypeDescriptorContext? context, Type sourceType) => sourceType == typeof(string);

		/// <inheritdoc/>
		public override object ConvertFrom(ITypeDescriptorContext? context, CultureInfo? culture, object value) => new NamespaceId((string)value);
	}
}
