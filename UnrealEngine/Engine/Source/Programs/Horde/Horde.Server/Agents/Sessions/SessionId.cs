// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using EpicGames.Core;
using Horde.Server.Utilities;
using MongoDB.Bson;

namespace Horde.Server.Agents.Sessions
{
	/// <summary>
	/// Identifier for a session
	/// </summary>
	/// <param name="Id">Id to construct from</param>
	[JsonSchemaString]
	[TypeConverter(typeof(ObjectIdTypeConverter<SessionId, SessionIdConverter>))]
	[ObjectIdConverter(typeof(SessionIdConverter))]
	public record struct SessionId(ObjectId Id)
	{
		/// <summary>
		/// Default empty value for session id
		/// </summary>
		public static SessionId Empty { get; } = default;

		/// <summary>
		/// Creates a new <see cref="SessionId"/>
		/// </summary>
		public static SessionId GenerateNewId() => new SessionId(ObjectId.GenerateNewId());

		/// <inheritdoc cref="ObjectId.Parse(System.String)"/>
		public static SessionId Parse(string text) => new SessionId(ObjectId.Parse(text));

		/// <inheritdoc cref="ObjectId.TryParse(System.String, out ObjectId)"/>
		public static bool TryParse(string text, out SessionId id)
		{
			if (ObjectId.TryParse(text, out ObjectId objectId))
			{
				id = new SessionId(objectId);
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
	class SessionIdConverter : ObjectIdConverter<SessionId>
	{
		/// <inheritdoc/>
		public override SessionId FromObjectId(ObjectId id) => new SessionId(id);

		/// <inheritdoc/>
		public override ObjectId ToObjectId(SessionId value) => value.Id;
	}
}
