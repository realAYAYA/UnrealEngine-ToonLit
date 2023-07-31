// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using EpicGames.Core;
using EpicGames.Serialization;
using MongoDB.Bson.Serialization;
using MongoDB.Bson.Serialization.Serializers;

namespace Horde.Build.Utilities
{
	/// <summary>
	/// Normalized string identifier for a resource
	/// </summary>
	[JsonSchemaString]
	[TypeConverter(typeof(StringIdTypeConverter))]
	[CbConverter(typeof(CbStringIdConverter<>))]
	public struct StringId<T> : IEquatable<StringId<T>>, IComparable<StringId<T>>
	{
		/// <summary>
		/// Empty string
		/// </summary>
		[SuppressMessage("Design", "CA1000:Do not declare static members on generic types", Justification = "<Pending>")]
		public static StringId<T> Empty { get; } = new StringId<T>(String.Empty);

		/// <summary>
		/// The text representing this id
		/// </summary>
		readonly string _text;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="input">Unique id for the string</param>
		[SuppressMessage("Globalization", "CA1308:Normalize strings to uppercase", Justification = "<Pending>")]
		public StringId(string input)
		{
			_text = input;

			if (_text.Length == 0)
			{
//				throw new ArgumentException("String id may not be empty");
			}

			const int MaxLength = 64;
			if (_text.Length > MaxLength)
			{
				throw new ArgumentException($"String id may not be longer than {MaxLength} characters");
			}

			for (int idx = 0; idx < _text.Length; idx++)
			{
				char character = _text[idx];
				if (!IsValidCharacter(character))
				{
					if (character >= 'A' && character <= 'Z')
					{
						_text = _text.ToLowerInvariant();
					}
					else
					{
						throw new ArgumentException($"{_text} is not a valid string id");
					}
				}
			}
		}

		/// <summary>
		/// Constructs from a nullable string
		/// </summary>
		/// <param name="text">The text to construct from</param>
		/// <returns></returns>
		[SuppressMessage("Design", "CA1000:Do not declare static members on generic types")]
		public static StringId<T>? FromNullable(string? text)
		{
			if (String.IsNullOrEmpty(text))
			{
				return null;
			}
			else
			{
				return new StringId<T>(text);
			}
		}

		/// <summary>
		/// Generates a new string id from the given text
		/// </summary>
		/// <param name="text">Text to generate from</param>
		/// <returns>New string id</returns>
		[SuppressMessage("Design", "CA1000:Do not declare static members on generic types", Justification = "<Pending>")]
		public static StringId<T> Sanitize(string text)
		{
			StringBuilder result = new StringBuilder();
			for (int idx = 0; idx < text.Length; idx++)
			{
				char character = text[idx];
				if (character >= 'A' && character <= 'Z')
				{
					result.Append((char)('a' + (character - 'A')));
				}
				else if (IsValidCharacter(character))
				{
					result.Append(character);
				}
				else if (result.Length > 0 && result[^1] != '-')
				{
					result.Append('-');
				}
			}
			while(result.Length > 0 && result[^1] == '-')
			{
				result.Remove(result.Length - 1, 1);
			}
			return new StringId<T>(result.ToString());
		}

		/// <summary>
		/// Checks whether this StringId is set
		/// </summary>
		public bool IsEmpty => String.IsNullOrEmpty(_text);

		/// <summary>
		/// Checks whether the given character is valid within a string id
		/// </summary>
		/// <param name="character">The character to check</param>
		/// <returns>True if the character is valid</returns>
		static bool IsValidCharacter(char character)
		{
			if (character >= 'a' && character <= 'z')
			{
				return true;
			}
			if (character >= '0' && character <= '9')
			{
				return true;
			}
			if (character == '-' || character == '_' || character == '.')
			{
				return true;
			}
			return false;
		}

		/// <inheritdoc/>
		public override bool Equals(object? obj)
		{
			return obj is StringId<T> id && Equals(id);
		}

		/// <inheritdoc/>
		public override int GetHashCode()
		{
			return _text.GetHashCode(StringComparison.Ordinal);
		}

		/// <inheritdoc/>
		public bool Equals(StringId<T> other)
		{
			return _text.Equals(other._text, StringComparison.Ordinal);
		}

		/// <inheritdoc/>
		public int CompareTo(StringId<T> other)
		{
			return String.CompareOrdinal(_text, other._text);
		}

		/// <inheritdoc/>
		public override string ToString()
		{
			return _text;
		}

		/// <summary>
		/// Compares two string ids for equality
		/// </summary>
		/// <param name="left">The first string id</param>
		/// <param name="right">Second string id</param>
		/// <returns>True if the two string ids are equal</returns>
		public static bool operator ==(StringId<T> left, StringId<T> right)
		{
			return left.Equals(right);
		}

		/// <summary>
		/// Compares two string ids for inequality
		/// </summary>
		/// <param name="left">The first string id</param>
		/// <param name="right">Second string id</param>
		/// <returns>True if the two string ids are not equal</returns>
		public static bool operator !=(StringId<T> left, StringId<T> right)
		{
			return !left.Equals(right);
		}
	}

	/// <summary>
	/// Converts <see cref="StringId{T}"/> values to and from JSON
	/// </summary>
	public class StringIdJsonConverter<T> : JsonConverter<StringId<T>>
	{
		/// <inheritdoc/>
		public override StringId<T> Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
		{
			return new StringId<T>(reader.GetString()!);
		}

		/// <inheritdoc/>
		public override void Write(Utf8JsonWriter writer, StringId<T> value, JsonSerializerOptions options)
		{
			writer.WriteStringValue(value.ToString());
		}
	}

	/// <summary>
	/// Converts <see cref="StringId{T}"/> values to and from JSON
	/// </summary>
	public class JsonStringIdConverterFactory : JsonConverterFactory
	{
		/// <inheritdoc/>
		public override bool CanConvert(Type typeToConvert)
		{
			return typeToConvert.IsGenericType && typeToConvert.GetGenericTypeDefinition() == typeof(StringId<>);
		}

		/// <inheritdoc/>
		public override JsonConverter? CreateConverter(Type type, JsonSerializerOptions options)
		{
			return (JsonConverter?)Activator.CreateInstance(typeof(StringIdJsonConverter<>).MakeGenericType(type.GetGenericArguments()));
		}
	}

	/// <summary>
	/// Serializer for StringId objects
	/// </summary>
	public sealed class StringIdBsonSerializer<T> : SerializerBase<StringId<T>>
	{
		/// <inheritdoc/>
		public override StringId<T> Deserialize(BsonDeserializationContext context, BsonDeserializationArgs args)
		{
			string argument;
			if (context.Reader.CurrentBsonType == MongoDB.Bson.BsonType.ObjectId)
			{
				argument = context.Reader.ReadObjectId().ToString();
			}
			else
			{
				argument = context.Reader.ReadString();
			}
			return new StringId<T>(argument);
		}

		/// <inheritdoc/>
		public override void Serialize(BsonSerializationContext context, BsonSerializationArgs args, StringId<T> value)
		{
			context.Writer.WriteString(value.ToString());
		}
	}

	/// <summary>
	/// Serializer for StringId objects
	/// </summary>
	public sealed class StringIdSerializationProvider : BsonSerializationProviderBase
	{
		/// <inheritdoc/>
		public override IBsonSerializer? GetSerializer(Type type, IBsonSerializerRegistry serializerRegistry)
		{
			if (type.IsGenericType && type.GetGenericTypeDefinition() == typeof(StringId<>))
			{
				return (IBsonSerializer?)Activator.CreateInstance(typeof(StringIdBsonSerializer<>).MakeGenericType(type.GetGenericArguments()));
			}
			else
			{
				return null;
			}
		}
	}

	sealed class CbStringIdConverter<T> : CbConverterBase<StringId<T>>
	{
		public override StringId<T> Read(CbField field)
		{
			return new StringId<T>(field.AsString().ToString());
		}

		public override void Write(CbWriter writer, StringId<T> value)
		{
			writer.WriteStringValue(value.ToString());
		}

		public override void WriteNamed(CbWriter writer, Utf8String name, StringId<T> value)
		{
			writer.WriteString(name, value.ToString());
		}
	}

	/// <summary>
	/// Type converter from strings to PropertyFilter objects
	/// </summary>
	sealed class StringIdTypeConverter : TypeConverter
	{
		readonly Type _type;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="type"></param>
		public StringIdTypeConverter(Type type)
		{
			_type = type;
		}

		/// <inheritdoc/>
		public override bool CanConvertFrom(ITypeDescriptorContext? context, Type sourceType)
		{
			return sourceType == typeof(string) || base.CanConvertFrom(context, sourceType);
		}

		/// <inheritdoc/>
		public override object ConvertFrom(ITypeDescriptorContext? context, CultureInfo? culture, object value)
		{
			return Activator.CreateInstance(_type, value)!;
		}

		/// <inheritdoc/>
		public override bool CanConvertTo(ITypeDescriptorContext? context, Type? destinationType)
		{
			if (destinationType == null)
			{
				return false;
			}
			if (destinationType == typeof(string))
			{
				return true;
			}
			if (destinationType.IsGenericType)
			{
				Type genericTypeDefinition = destinationType.GetGenericTypeDefinition();
				if (genericTypeDefinition == typeof(StringId<>))
				{
					return true;
				}
				if (genericTypeDefinition == typeof(Nullable<>))
				{
					return CanConvertTo(context, genericTypeDefinition.GetGenericArguments()[0]);
				}
			}
			return false;
		}

		/// <inheritdoc/>
		public override object? ConvertTo(ITypeDescriptorContext? context, CultureInfo? culture, object? value, Type destinationType)
		{
			if (destinationType == typeof(string))
			{
				return value?.ToString();
			}
			else
			{
				return Activator.CreateInstance(destinationType, value);
			}
		}
	}
}
