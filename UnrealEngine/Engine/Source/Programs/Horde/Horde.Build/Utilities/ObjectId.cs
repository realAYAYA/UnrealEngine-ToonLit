// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using System.Text.Json;
using System.Text.Json.Serialization;
using EpicGames.Core;
using EpicGames.Serialization;
using MongoDB.Bson;
using MongoDB.Bson.Serialization;
using MongoDB.Bson.Serialization.Serializers;

namespace Horde.Build.Utilities
{
	/// <summary>
	/// Normalized string identifier for a resource
	/// </summary>
	[JsonSchemaString]
	[JsonConverter(typeof(JsonObjectIdConverterFactory))]
	[TypeConverter(typeof(ObjectIdTypeConverter))]
	public struct ObjectId<T> : IEquatable<ObjectId<T>>, IComparable<ObjectId<T>>
	{
		/// <summary>
		/// Empty string
		/// </summary>
		[SuppressMessage("Design", "CA1000:Do not declare static members on generic types", Justification = "<Pending>")]
		public static ObjectId<T> Empty { get; } = new ObjectId<T>(ObjectId.Empty);

		/// <summary>
		/// The text representing this id
		/// </summary>
		public ObjectId Value { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public ObjectId(ObjectId value) => Value = value;

		/// <summary>
		/// Constructor
		/// </summary>
		public ObjectId(byte[] bytes) => Value = new ObjectId(bytes);

		/// <summary>
		/// Constructor
		/// </summary>
		public ObjectId(string value) => Value = new ObjectId(value);

		/// <inheritdoc cref="ObjectId.GenerateNewId()"/>
		[SuppressMessage("Design", "CA1000:Do not declare static members on generic types", Justification = "<Pending>")]
		public static ObjectId<T> GenerateNewId() => new ObjectId<T>(ObjectId.GenerateNewId());

		/// <inheritdoc cref="ObjectId.Parse(String)"/>
		[SuppressMessage("Design", "CA1000:Do not declare static members on generic types", Justification = "<Pending>")]
		public static ObjectId<T> Parse(string str) => new ObjectId<T>(ObjectId.Parse(str));

		/// <inheritdoc cref="ObjectId.TryParse(String, out ObjectId)"/>
		[SuppressMessage("Design", "CA1000:Do not declare static members on generic types", Justification = "<Pending>")]
		public static bool TryParse(string str, out ObjectId<T> value)
		{
			if (ObjectId.TryParse(str, out ObjectId id))
			{
				value = new ObjectId<T>(id);
				return true;
			}
			else
			{
				value = Empty;
				return false;
			}
		}

		/// <inheritdoc/>
		public override bool Equals(object? obj) => obj is ObjectId<T> other && Equals(other);

		/// <inheritdoc/>
		public override int GetHashCode() => Value.GetHashCode();

		/// <inheritdoc/>
		public bool Equals(ObjectId<T> other) => Value.Equals(other.Value);

		/// <inheritdoc/>
		public override string ToString() => Value.ToString();

		/// <inheritdoc/>
		public int CompareTo(ObjectId<T> other) => Value.CompareTo(other.Value);

		/// <inheritdoc cref="ObjectId.op_LessThan"/>
		public static bool operator <(ObjectId<T> left, ObjectId<T> right) => left.Value < right.Value;

		/// <inheritdoc cref="ObjectId.op_GreaterThan"/>
		public static bool operator >(ObjectId<T> left, ObjectId<T> right) => left.Value > right.Value;

		/// <inheritdoc cref="ObjectId.op_LessThanOrEqual"/>
		public static bool operator <=(ObjectId<T> left, ObjectId<T> right) => left.Value <= right.Value;

		/// <inheritdoc cref="ObjectId.op_GreaterThanOrEqual"/>
		public static bool operator >=(ObjectId<T> left, ObjectId<T> right) => left.Value >= right.Value;

		/// <inheritdoc cref="ObjectId.op_Equality"/>
		public static bool operator ==(ObjectId<T> left, ObjectId<T> right) => left.Value == right.Value;

		/// <inheritdoc cref="ObjectId.op_Inequality"/>
		public static bool operator !=(ObjectId<T> left, ObjectId<T> right) => left.Value != right.Value;
	}

	/// <summary>
	/// Converts <see cref="ObjectId{T}"/> values to and from JSON
	/// </summary>
	public class ObjectIdJsonConverter<T> : JsonConverter<ObjectId<T>>
	{
		/// <inheritdoc/>
		public override ObjectId<T> Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
		{
			return new ObjectId<T>(ObjectId.Parse(reader.GetString()!));
		}

		/// <inheritdoc/>
		public override void Write(Utf8JsonWriter writer, ObjectId<T> value, JsonSerializerOptions options)
		{
			writer.WriteStringValue(value.ToString());
		}
	}

	/// <summary>
	/// Converts <see cref="ObjectId{T}"/> values to and from JSON
	/// </summary>
	public class JsonObjectIdConverterFactory : JsonConverterFactory
	{
		/// <inheritdoc/>
		public override bool CanConvert(Type typeToConvert)
		{
			return typeToConvert.IsGenericType && typeToConvert.GetGenericTypeDefinition() == typeof(ObjectId<>);
		}

		/// <inheritdoc/>
		public override JsonConverter? CreateConverter(Type type, JsonSerializerOptions options)
		{
			return (JsonConverter?)Activator.CreateInstance(typeof(ObjectIdJsonConverter<>).MakeGenericType(type.GetGenericArguments()));
		}
	}

	/// <summary>
	/// Serializer for ObjectId objects
	/// </summary>
	public sealed class ObjectIdBsonSerializer<T> : SerializerBase<ObjectId<T>>
	{
		/// <inheritdoc/>
		public override ObjectId<T> Deserialize(BsonDeserializationContext context, BsonDeserializationArgs args)
		{
			return new ObjectId<T>(context.Reader.ReadObjectId());
		}

		/// <inheritdoc/>
		public override void Serialize(BsonSerializationContext context, BsonSerializationArgs args, ObjectId<T> value)
		{
			context.Writer.WriteObjectId(value.Value);
		}
	}

	/// <summary>
	/// Serializer for ObjectId objects
	/// </summary>
	public sealed class ObjectIdSerializationProvider : BsonSerializationProviderBase
	{
		/// <inheritdoc/>
		public override IBsonSerializer? GetSerializer(Type type, IBsonSerializerRegistry serializerRegistry)
		{
			if (type.IsGenericType && type.GetGenericTypeDefinition() == typeof(ObjectId<>))
			{
				return (IBsonSerializer?)Activator.CreateInstance(typeof(ObjectIdBsonSerializer<>).MakeGenericType(type.GetGenericArguments()));
			}
			else
			{
				return null;
			}
		}
	}

	/// <summary>
	/// Type converter from strings to PropertyFilter objects
	/// </summary>
	sealed class ObjectIdTypeConverter : TypeConverter
	{
		readonly Type _type;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="type"></param>
		public ObjectIdTypeConverter(Type type)
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
				if (genericTypeDefinition == typeof(ObjectId<>))
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
