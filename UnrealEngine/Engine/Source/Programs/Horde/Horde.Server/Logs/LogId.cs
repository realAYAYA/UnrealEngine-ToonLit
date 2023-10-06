// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using EpicGames.Core;
using Horde.Server.Utilities;
using MongoDB.Bson;

namespace Horde.Server.Logs
{
	/// <summary>
	/// Identifier for a log
	/// </summary>
	/// <param name="Id">Id to construct from</param>
	[JsonSchemaString]
	[TypeConverter(typeof(ObjectIdTypeConverter<LogId, LogIdConverter>))]
	[ObjectIdConverter(typeof(LogIdConverter))]
	public record struct LogId(ObjectId Id)
	{
		/// <summary>
		/// Constant value for empty user id
		/// </summary>
		public static LogId Empty { get; } = new LogId(ObjectId.Empty);

		/// <summary>
		/// Creates a new <see cref="LogId"/>
		/// </summary>
		public static LogId GenerateNewId() => new LogId(ObjectId.GenerateNewId());

		/// <inheritdoc cref="ObjectId.Parse(System.String)"/>
		public static LogId Parse(string text) => new LogId(ObjectId.Parse(text));

		/// <inheritdoc cref="ObjectId.TryParse(System.String, out ObjectId)"/>
		public static bool TryParse(string text, out LogId id)
		{
			if (ObjectId.TryParse(text, out ObjectId objectId))
			{
				id = new LogId(objectId);
				return true;
			}
			else
			{
				id = default;
				return false;
			}
		}

		/// <inheritdoc/>
		public override string ToString() => Id.ToString();
	}

	/// <summary>
	/// Converter to and from <see cref="ObjectId"/> instances.
	/// </summary>
	class LogIdConverter : ObjectIdConverter<LogId>
	{
		/// <inheritdoc/>
		public override LogId FromObjectId(ObjectId id) => new LogId(id);

		/// <inheritdoc/>
		public override ObjectId ToObjectId(LogId value) => value.Id;
	}
}
