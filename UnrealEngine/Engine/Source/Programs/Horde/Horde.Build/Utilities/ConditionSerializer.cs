// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using EpicGames.Horde.Common;
using MongoDB.Bson;
using MongoDB.Bson.Serialization;

namespace Horde.Build.Utilities
{
	/// <summary>
	/// Serializer for Condition objects
	/// </summary>
	public sealed class ConditionSerializer : IBsonSerializer<Condition>
	{
		/// <inheritdoc/>
		public Type ValueType => typeof(Condition);

		/// <inheritdoc/>
		void IBsonSerializer.Serialize(BsonSerializationContext context, BsonSerializationArgs args, object value) => Serialize(context, args, (Condition)value);

		/// <inheritdoc/>
		object IBsonSerializer.Deserialize(BsonDeserializationContext context, BsonDeserializationArgs args) => Deserialize(context, args);

		/// <inheritdoc/>
		public Condition Deserialize(BsonDeserializationContext context, BsonDeserializationArgs args)
		{
			if(context.Reader.CurrentBsonType == BsonType.Null)
			{
				context.Reader.ReadNull();
				return null!;
			}
			return Condition.Parse(context.Reader.ReadString());
		}

		/// <inheritdoc/>
		public void Serialize(BsonSerializationContext context, BsonSerializationArgs args, Condition value)
		{
			context.Writer.WriteString(value.ToString());
		}
	}
}
