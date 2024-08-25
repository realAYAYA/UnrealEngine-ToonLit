// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using EpicGames.Core;

namespace EpicGames.Horde.Jobs
{
	/// <summary>
	/// Identifier for a job step batch
	/// </summary>
	/// <param name="SubResourceId">Id to construct from</param>
	[LogValueType]
	[JsonSchemaString]
	[TypeConverter(typeof(SubResourceIdTypeConverter<JobStepBatchId, JobStepBatchIdConverter>))]
	[SubResourceIdConverter(typeof(JobStepBatchIdConverter))]
	public record struct JobStepBatchId(SubResourceId SubResourceId)
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public JobStepBatchId(ushort value) : this(new SubResourceId(value))
		{ }

		/// <summary>
		/// Creates a new <see cref="JobId"/>
		/// </summary>
		public static JobStepBatchId GenerateNewId() => new JobStepBatchId(SubResourceId.GenerateNewId());

		/// <inheritdoc cref="SubResourceId.Parse(System.String)"/>
		public static JobStepBatchId Parse(string text) => new JobStepBatchId(SubResourceId.Parse(text));

		/// <inheritdoc/>
		public override string ToString() => SubResourceId.ToString();
	}

	/// <summary>
	/// Converter to and from <see cref="SubResourceId"/> instances.
	/// </summary>
	class JobStepBatchIdConverter : SubResourceIdConverter<JobStepBatchId>
	{
		/// <inheritdoc/>
		public override JobStepBatchId FromSubResourceId(SubResourceId subResourceId) => new JobStepBatchId(subResourceId);

		/// <inheritdoc/>
		public override SubResourceId ToSubResourceId(JobStepBatchId value) => value.SubResourceId;
	}
}
