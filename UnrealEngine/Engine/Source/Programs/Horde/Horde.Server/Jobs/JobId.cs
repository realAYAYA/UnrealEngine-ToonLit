// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using EpicGames.Core;
using Horde.Server.Utilities;
using MongoDB.Bson;

namespace Horde.Server.Jobs
{
	/// <summary>
	/// Identifier for a job
	/// </summary>
	/// <param name="Id">Id to construct from</param>
	[JsonSchemaString]
	[TypeConverter(typeof(ObjectIdTypeConverter<JobId, JobIdConverter>))]
	[ObjectIdConverter(typeof(JobIdConverter))]
	public record struct JobId(ObjectId Id)
	{
		/// <summary>
		/// Constant value for an empty job id
		/// </summary>
		public static JobId Empty { get; } = default;

		/// <summary>
		/// Creates a new <see cref="JobId"/>
		/// </summary>
		public static JobId GenerateNewId() => new JobId(ObjectId.GenerateNewId());

		/// <inheritdoc cref="ObjectId.Parse(System.String)"/>
		public static JobId Parse(string text) => new JobId(ObjectId.Parse(text));

		/// <inheritdoc/>
		public override string ToString() => Id.ToString();
	}

	/// <summary>
	/// Converter to and from <see cref="ObjectId"/> instances.
	/// </summary>
	class JobIdConverter : ObjectIdConverter<JobId>
	{
		/// <inheritdoc/>
		public override JobId FromObjectId(ObjectId id) => new JobId(id);

		/// <inheritdoc/>
		public override ObjectId ToObjectId(JobId value) => value.Id;
	}
}
