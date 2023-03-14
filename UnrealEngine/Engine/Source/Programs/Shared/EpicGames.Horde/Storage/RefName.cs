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
	[JsonConverter(typeof(RefNameJsonConverter))]
	[TypeConverter(typeof(RefNameTypeConverter))]
	[CbConverter(typeof(RefNameCbConverter))]
	public struct RefName : IEquatable<RefName>, IComparable<RefName>
	{
		/// <summary>
		/// Empty ref name
		/// </summary>
		public static RefName Empty { get; } = default;

		/// <summary>
		/// String for the ref name
		/// </summary>
		public Utf8String Text { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="text"></param>
		public RefName(Utf8String text)
		{
			Text = text;
			ContentId.ValidateArgument(nameof(text), text);
		}

		/// <inheritdoc/>
		public override bool Equals(object? obj) => obj is RefName refId && Equals(refId);

		/// <inheritdoc/>
		public override int GetHashCode() => Text.GetHashCode();

		/// <inheritdoc/>
		public bool Equals(RefName refName) => Text == refName.Text;

		/// <inheritdoc/>
		public int CompareTo(RefName other) => Text.CompareTo(other.Text);

		/// <inheritdoc/>
		public override string ToString() => Text.ToString();

		/// <inheritdoc/>
		public static bool operator ==(RefName lhs, RefName rhs) => lhs.Equals(rhs);

		/// <inheritdoc/>
		public static bool operator !=(RefName lhs, RefName rhs) => !lhs.Equals(rhs);

		/// <inheritdoc/>
		public static bool operator <(RefName lhs, RefName rhs) => lhs.CompareTo(rhs) < 0;

		/// <inheritdoc/>
		public static bool operator <=(RefName lhs, RefName rhs) => lhs.CompareTo(rhs) <= 0;

		/// <inheritdoc/>
		public static bool operator >(RefName lhs, RefName rhs) => lhs.CompareTo(rhs) > 0;

		/// <inheritdoc/>
		public static bool operator >=(RefName lhs, RefName rhs) => lhs.CompareTo(rhs) >= 0;

	}

	/// <summary>
	/// Type converter for IoHash to and from JSON
	/// </summary>
	sealed class RefNameJsonConverter : JsonConverter<RefName>
	{
		/// <inheritdoc/>
		public override RefName Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options) => new RefName(new Utf8String(reader.ValueSpan.ToArray()));

		/// <inheritdoc/>
		public override void Write(Utf8JsonWriter writer, RefName value, JsonSerializerOptions options) => writer.WriteStringValue(value.Text.Span);
	}

	/// <summary>
	/// Type converter from strings to IoHash objects
	/// </summary>
	sealed class RefNameTypeConverter : TypeConverter
	{
		/// <inheritdoc/>
		public override bool CanConvertFrom(ITypeDescriptorContext? context, Type sourceType)
		{
			return sourceType == typeof(string);
		}

		/// <inheritdoc/>
		public override object ConvertFrom(ITypeDescriptorContext? context, CultureInfo? culture, object? value)
		{
			return new RefName((string)value!);
		}
	}

	/// <summary>
	/// Type converter to compact binary
	/// </summary>
	sealed class RefNameCbConverter : CbConverterBase<RefName>
	{
		/// <inheritdoc/>
		public override RefName Read(CbField field) => new RefName(field.AsUtf8String());

		/// <inheritdoc/>
		public override void Write(CbWriter writer, RefName value) => writer.WriteUtf8StringValue(value.Text);

		/// <inheritdoc/>
		public override void WriteNamed(CbWriter writer, Utf8String name, RefName value) => writer.WriteUtf8String(name, value.Text);
	}
}
