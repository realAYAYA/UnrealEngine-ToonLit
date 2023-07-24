// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Serialization;
using System;
using System.ComponentModel;
using System.Globalization;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Identifier for a channel for receiving compute responses
	/// </summary>
	[CbConverter(typeof(ChannelIdCbConverter))]
	[JsonConverter(typeof(ChannelIdJsonConverter))]
	[TypeConverter(typeof(ChannelIdTypeConverter))]
	public struct ChannelId : IEquatable<ChannelId>
	{
		/// <summary>
		/// The text representing this id
		/// </summary>
		readonly StringId _inner;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="input">Unique id for the string</param>
		public ChannelId(string input)
		{
			_inner = new StringId(input);
		}

		/// <inheritdoc/>
		public override bool Equals(object? obj) => obj is ChannelId id && _inner.Equals(id._inner);

		/// <inheritdoc/>
		public override int GetHashCode() => _inner.GetHashCode();

		/// <inheritdoc/>
		public bool Equals(ChannelId other) => _inner.Equals(other._inner);

		/// <inheritdoc/>
		public override string ToString() => _inner.ToString();

		/// <inheritdoc cref="StringId.op_Equality"/>
		public static bool operator ==(ChannelId left, ChannelId right) => left._inner == right._inner;

		/// <inheritdoc cref="StringId.op_Inequality"/>
		public static bool operator !=(ChannelId left, ChannelId right) => left._inner != right._inner;
	}

	/// <summary>
	/// Compact binary converter for ChannelId
	/// </summary>
	sealed class ChannelIdCbConverter : CbConverterBase<ChannelId>
	{
		/// <inheritdoc/>
		public override ChannelId Read(CbField field) => new ChannelId(field.AsString());

		/// <inheritdoc/>
		public override void Write(CbWriter writer, ChannelId value) => writer.WriteStringValue(value.ToString());

		/// <inheritdoc/>
		public override void WriteNamed(CbWriter writer, Utf8String name, ChannelId value) => writer.WriteString(name, value.ToString());
	}

	/// <summary>
	/// Type converter for ChannelId to and from JSON
	/// </summary>
	sealed class ChannelIdJsonConverter : JsonConverter<ChannelId>
	{
		/// <inheritdoc/>
		public override ChannelId Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options) => new ChannelId(reader.GetString() ?? String.Empty);

		/// <inheritdoc/>
		public override void Write(Utf8JsonWriter writer, ChannelId value, JsonSerializerOptions options) => writer.WriteStringValue(value.ToString());
	}

	/// <summary>
	/// Type converter from strings to ChannelId objects
	/// </summary>
	sealed class ChannelIdTypeConverter : TypeConverter
	{
		/// <inheritdoc/>
		public override bool CanConvertFrom(ITypeDescriptorContext? context, Type? sourceType) => sourceType == typeof(string);

		/// <inheritdoc/>
		public override object ConvertFrom(ITypeDescriptorContext? context, CultureInfo? culture, object? value) => new ChannelId((string)value!);
	}
}
