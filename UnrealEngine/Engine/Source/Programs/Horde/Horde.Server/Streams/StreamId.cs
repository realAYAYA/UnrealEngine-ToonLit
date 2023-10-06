// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Serialization;
using Horde.Server.Utilities;

namespace Horde.Server.Streams
{
	/// <summary>
	/// Identifier for a stream
	/// </summary>
	/// <param name="Id">Id to construct from</param>
	[JsonSchemaString]
	[TypeConverter(typeof(StringIdTypeConverter<StreamId, StreamIdConverter>))]
	[StringIdConverter(typeof(StreamIdConverter))]
	[CbConverter(typeof(StringIdCbConverter<StreamId, StreamIdConverter>))]
	public record struct StreamId(StringId Id)
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public StreamId(string id) : this(new StringId(id))
		{
		}

		/// <inheritdoc cref="StringId.IsEmpty"/>
		public bool IsEmpty => Id.IsEmpty;

		/// <inheritdoc/>
		public override string ToString() => Id.ToString();
	}

	/// <summary>
	/// Converter to and from <see cref="StringId"/> instances.
	/// </summary>
	class StreamIdConverter : StringIdConverter<StreamId>
	{
		/// <inheritdoc/>
		public override StreamId FromStringId(StringId id) => new StreamId(id);

		/// <inheritdoc/>
		public override StringId ToStringId(StreamId value) => value.Id;
	}
}
