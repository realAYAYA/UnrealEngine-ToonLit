// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel;
using System.Globalization;
using EpicGames.Redis;
using MongoDB.Bson.Serialization;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Bson.Serialization.Serializers;
using StackExchange.Redis;

namespace Horde.Build.Agents
{
	/// <summary>
	/// Normalized hostname of an agent
	/// </summary>
	[TypeConverter(typeof(AgentIdTypeConverter))]
	[RedisConverter(typeof(AgentIdRedisConverter))]
	[BsonSerializer(typeof(AgentIdBsonSerializer))]
	public struct AgentId : IEquatable<AgentId>
	{
		/// <summary>
		/// The text representing this id
		/// </summary>
		readonly string _name;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="name">Hostname of the agent</param>
		public AgentId(string name)
		{
			if (name.Length == 0)
			{
				throw new ArgumentException("Agent name may not be empty");
			}

			char[] result = new char[name.Length];
			for (int idx = 0; idx < name.Length; idx++)
			{
				char character = name[idx];
				if ((character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') || character == '_' || character == '-')
				{
					result[idx] = character;
				}
				else if (character >= 'a' && character <= 'z')
				{
					result[idx] = (char)('A' + (character - 'a'));
				}
				else if (character == '.')
				{
					if (idx == 0)
					{
						throw new ArgumentException("Agent name may not begin with the '.' character");
					}
					else if (idx == name.Length - 1)
					{
						throw new ArgumentException("Agent name may not end with the '.' character");
					}
					else if (name[idx - 1] == '.')
					{
						throw new ArgumentException("Agent name may not contain two consecutive '.' characters");
					}
					else
					{
						result[idx] = character;
					}
				}
				else
				{
					throw new ArgumentException($"Character '{character}' in agent '{name}' is invalid");
				}
			}
			_name = new string(result);
		}

		/// <inheritdoc/>
		public override bool Equals(object? obj)
		{
			return obj is AgentId id && Equals(id);
		}

		/// <inheritdoc/>
		public override int GetHashCode()
		{
			return _name.GetHashCode(StringComparison.Ordinal);
		}

		/// <inheritdoc/>
		public bool Equals(AgentId other)
		{
			return _name.Equals(other._name, StringComparison.Ordinal);
		}

		/// <inheritdoc/>
		public override string ToString()
		{
			return _name;
		}

		/// <summary>
		/// Compares two string ids for equality
		/// </summary>
		/// <param name="left">The first string id</param>
		/// <param name="right">Second string id</param>
		/// <returns>True if the two string ids are equal</returns>
		public static bool operator ==(AgentId left, AgentId right)
		{
			return left.Equals(right);
		}

		/// <summary>
		/// Compares two string ids for inequality
		/// </summary>
		/// <param name="left">The first string id</param>
		/// <param name="right">Second string id</param>
		/// <returns>True if the two string ids are not equal</returns>
		public static bool operator !=(AgentId left, AgentId right)
		{
			return !left.Equals(right);
		}
	}

	/// <summary>
	/// Converter to/from Redis values
	/// </summary>
	public sealed class AgentIdRedisConverter : IRedisConverter<AgentId>
	{
		/// <inheritdoc/>
		public AgentId FromRedisValue(RedisValue value) => new AgentId((string)value!);

		/// <inheritdoc/>
		public RedisValue ToRedisValue(AgentId value) => value.ToString();
	}

	/// <summary>
	/// Serializer for StringId objects
	/// </summary>
	public sealed class AgentIdBsonSerializer : SerializerBase<AgentId>
	{
		/// <inheritdoc/>
		public override AgentId Deserialize(BsonDeserializationContext context, BsonDeserializationArgs args)
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
			return new AgentId(argument);
		}

		/// <inheritdoc/>
		public override void Serialize(BsonSerializationContext context, BsonSerializationArgs args, AgentId value)
		{
			context.Writer.WriteString(value.ToString());
		}
	}

	/// <summary>
	/// Type converter from strings to PropertyFilter objects
	/// </summary>
	sealed class AgentIdTypeConverter : TypeConverter
	{
		/// <inheritdoc/>
		public override bool CanConvertFrom(ITypeDescriptorContext? context, Type sourceType)
		{
			return sourceType == typeof(string);
		}

		/// <inheritdoc/>
		public override object ConvertFrom(ITypeDescriptorContext? context, CultureInfo? culture, object value)
		{
			return new AgentId((string)value);
		}

		/// <inheritdoc/>
		public override bool CanConvertTo(ITypeDescriptorContext? context, Type? destinationType)
		{
			return destinationType == typeof(string);
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
				return null;
			}
		}
	}
}
