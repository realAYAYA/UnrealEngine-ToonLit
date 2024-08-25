// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using EpicGames.Core;
using ProtoBuf;
using StackExchange.Redis;

namespace EpicGames.Redis.Converters
{
	/// <summary>
	/// Converter for records to Redis values using ProtoBuf-Net annotations.
	/// </summary>
	/// <typeparam name="T">The record type</typeparam>
	public class RedisProtobufConverter<T> : IRedisConverter<T>
	{
		/// <inheritdoc/>
		public RedisValue ToRedisValue(T value)
		{
			using (MemoryStream stream = new MemoryStream())
			{
				Serializer.Serialize(stream, value);
				return stream.ToArray();
			}
		}

		/// <inheritdoc/>
		public T FromRedisValue(RedisValue value)
		{
			using (ReadOnlyMemoryStream stream = new ReadOnlyMemoryStream(value))
			{
				T result = Serializer.Deserialize<T>(stream);
				return result;
			}
		}
	}
}
