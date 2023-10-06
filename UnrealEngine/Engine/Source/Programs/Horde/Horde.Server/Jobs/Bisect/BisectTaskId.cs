// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using EpicGames.Core;
using Horde.Server.Utilities;
using MongoDB.Bson;

namespace Horde.Server.Jobs.Bisect
{
	/// <summary>
	/// Identifier for a job
	/// </summary>
	/// <param name="Id">Id to construct from</param>
	[JsonSchemaString]
	[TypeConverter(typeof(ObjectIdTypeConverter<BisectTaskId, BisectTaskIdConverter>))]
	[ObjectIdConverter(typeof(BisectTaskIdConverter))]
	public record struct BisectTaskId(ObjectId Id)
	{
		/// <summary>
		/// Creates a new <see cref="BisectTaskId"/>
		/// </summary>
		public static BisectTaskId GenerateNewId() => new BisectTaskId(ObjectId.GenerateNewId());

		/// <inheritdoc cref="ObjectId.Parse(System.String)"/>
		public static BisectTaskId Parse(string text) => new BisectTaskId(ObjectId.Parse(text));

		/// <inheritdoc/>
		public override string ToString() => Id.ToString();
	}

	/// <summary>
	/// Converter to and from <see cref="ObjectId"/> instances.
	/// </summary>
	class BisectTaskIdConverter : ObjectIdConverter<BisectTaskId>
	{
		/// <inheritdoc/>
		public override BisectTaskId FromObjectId(ObjectId id) => new BisectTaskId(id);

		/// <inheritdoc/>
		public override ObjectId ToObjectId(BisectTaskId value) => value.Id;
	}
}
