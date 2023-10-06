// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using EpicGames.Core;
using Horde.Server.Utilities;
using MongoDB.Bson;

namespace Horde.Server.Tools
{
	/// <summary>
	/// Identifier for a tool deployment
	/// </summary>
	/// <param name="Id">Id to construct from</param>
	[JsonSchemaString]
	[TypeConverter(typeof(ObjectIdTypeConverter<ToolDeploymentId, ToolDeploymentIdConverter>))]
	[ObjectIdConverter(typeof(ToolDeploymentIdConverter))]
	public record struct ToolDeploymentId(ObjectId Id)
	{
		/// <summary>
		/// Creates a new <see cref="ToolDeploymentId"/>
		/// </summary>
		public static ToolDeploymentId GenerateNewId() => new ToolDeploymentId(ObjectId.GenerateNewId());

		/// <inheritdoc/>
		public override string ToString() => Id.ToString();
	}

	/// <summary>
	/// Converter to and from <see cref="ObjectId"/> instances.
	/// </summary>
	class ToolDeploymentIdConverter : ObjectIdConverter<ToolDeploymentId>
	{
		/// <inheritdoc/>
		public override ToolDeploymentId FromObjectId(ObjectId id) => new ToolDeploymentId(id);

		/// <inheritdoc/>
		public override ObjectId ToObjectId(ToolDeploymentId value) => value.Id;
	}
}
