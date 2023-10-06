// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using Horde.Server.Utilities;
using MongoDB.Bson;

namespace Horde.Server.Artifacts
{
	/// <summary>
	/// Unique id for an artifact
	/// </summary>
	/// <param name="Id">Identifier for the artifact</param>
	[TypeConverter(typeof(ObjectIdTypeConverter<ArtifactId, ArtifactIdConverter>))]
	[ObjectIdConverter(typeof(ArtifactIdConverter))]
	public record struct ArtifactId(ObjectId Id)
	{
		/// <summary>
		/// Creates a new random artifact id
		/// </summary>
		/// <returns>New artifact id</returns>
		public static ArtifactId GenerateNewId() => new ArtifactId(ObjectId.GenerateNewId());

		/// <inheritdoc cref="ObjectId.Parse(System.String)"/>
		public static ArtifactId Parse(string text) => new ArtifactId(ObjectId.Parse(text));

		/// <inheritdoc/>
		public override string ToString() => Id.ToString();
	}

	/// <summary>
	/// Converter class to and from ObjectId values
	/// </summary>
	class ArtifactIdConverter : ObjectIdConverter<ArtifactId>
	{
		/// <inheritdoc/>
		public override ArtifactId FromObjectId(ObjectId id) => new ArtifactId(id);

		/// <inheritdoc/>
		public override ObjectId ToObjectId(ArtifactId value) => value.Id;
	}
}
