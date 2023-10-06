// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using Horde.Server.Agents;
using Horde.Server.Agents.Pools;
using Horde.Server.Jobs.Bisect;
using Horde.Server.Logs;
using Horde.Server.Streams;
using HordeCommon;
using MongoDB.Bson.Serialization;
using MongoDB.Bson.Serialization.Attributes;

namespace Horde.Server.Jobs
{
	/// <summary>
	/// Unique id struct for JobStepRef objects. Includes a job id, batch id, and step id to uniquely identify the step.
	/// </summary>
	[BsonSerializer(typeof(JobStepRefIdSerializer))]
	public struct JobStepRefId : IEquatable<JobStepRefId>, IComparable<JobStepRefId>
	{
		/// <summary>
		/// The job id
		/// </summary>
		public JobId JobId { get; set; }

		/// <summary>
		/// The batch id within the job
		/// </summary>
		public SubResourceId BatchId { get; set; }

		/// <summary>
		/// The step id
		/// </summary>
		public SubResourceId StepId { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="jobId">The job id</param>
		/// <param name="batchId">The batch id within the job</param>
		/// <param name="stepId">The step id</param>
		public JobStepRefId(JobId jobId, SubResourceId batchId, SubResourceId stepId)
		{
			JobId = jobId;
			BatchId = batchId;
			StepId = stepId;
		}

		/// <summary>
		/// Parse a job step id from a string
		/// </summary>
		/// <param name="text">Text to parse</param>
		/// <returns>The parsed id</returns>
		public static JobStepRefId Parse(string text)
		{
			string[] components = text.Split(':');
			return new JobStepRefId(JobId.Parse(components[0]), SubResourceId.Parse(components[1]), SubResourceId.Parse(components[2]));
		}

		/// <summary>
		/// Formats this id as a string
		/// </summary>
		/// <returns>Formatted id</returns>
		public override string ToString()
		{
			return $"{JobId}:{BatchId}:{StepId}";
		}

		/// <inheritdoc/>
		public override int GetHashCode() => HashCode.Combine(JobId, BatchId, StepId);

		/// <inheritdoc/>
		public override bool Equals([NotNullWhen(true)] object? obj) => obj is JobStepRefId other && Equals(other);

		/// <inheritdoc/>
		public bool Equals(JobStepRefId other)
		{
			return JobId.Equals(other.JobId) && BatchId.Equals(other.BatchId) && StepId.Equals(other.StepId);
		}

		/// <inheritdoc/>
		public int CompareTo(JobStepRefId other)
		{
			int result = JobId.Id.CompareTo(other.JobId.Id);
			if (result != 0)
			{
				return result;
			}

			result = BatchId.Value.CompareTo(other.BatchId.Value);
			if (result != 0)
			{
				return result;
			}

			return StepId.Value.CompareTo(other.StepId.Value);
		}

		/// <inheritdoc/>
		public static bool operator ==(JobStepRefId lhs, JobStepRefId rhs) => lhs.Equals(rhs);

		/// <inheritdoc/>
		public static bool operator !=(JobStepRefId lhs, JobStepRefId rhs) => !lhs.Equals(rhs);

		/// <inheritdoc/>
		public static bool operator <(JobStepRefId lhs, JobStepRefId rhs) => lhs.CompareTo(rhs) < 0;

		/// <inheritdoc/>
		public static bool operator >(JobStepRefId lhs, JobStepRefId rhs) => lhs.CompareTo(rhs) > 0;

		/// <inheritdoc/>
		public static bool operator <=(JobStepRefId lhs, JobStepRefId rhs) => lhs.CompareTo(rhs) <= 0;

		/// <inheritdoc/>
		public static bool operator >=(JobStepRefId lhs, JobStepRefId rhs) => lhs.CompareTo(rhs) >= 0;
	}

	/// <summary>
	/// Serializer for JobStepRefId objects
	/// </summary>
	public sealed class JobStepRefIdSerializer : IBsonSerializer<JobStepRefId>
	{
		/// <inheritdoc/>
		public Type ValueType => typeof(JobStepRefId);

		/// <inheritdoc/>
		void IBsonSerializer.Serialize(BsonSerializationContext context, BsonSerializationArgs args, object value)
		{
			Serialize(context, args, (JobStepRefId)value);
		}

		/// <inheritdoc/>
		object IBsonSerializer.Deserialize(BsonDeserializationContext context, BsonDeserializationArgs args)
		{
			return ((IBsonSerializer<JobStepRefId>)this).Deserialize(context, args);
		}

		/// <inheritdoc/>
		public JobStepRefId Deserialize(BsonDeserializationContext context, BsonDeserializationArgs args)
		{
			return JobStepRefId.Parse(context.Reader.ReadString());
		}

		/// <inheritdoc/>
		public void Serialize(BsonSerializationContext context, BsonSerializationArgs args, JobStepRefId value)
		{
			context.Writer.WriteString(((JobStepRefId)value).ToString());
		}
	}

	/// <summary>
	/// Searchable reference to a jobstep
	/// </summary>
	public interface IJobStepRef
	{
		/// <summary>
		/// Globally unique identifier for the jobstep being referenced
		/// </summary>
		public JobStepRefId Id { get; }

		/// <summary>
		/// Name of the job
		/// </summary>
		public string JobName { get; }

		/// <summary>
		/// Name of the name
		/// </summary>
		public string NodeName { get; }

		/// <summary>
		/// Unique id of the stream containing the job
		/// </summary>
		public StreamId StreamId { get; }

		/// <summary>
		/// Template for the job being executed
		/// </summary>
		public TemplateId TemplateId { get; }

		/// <summary>
		/// The change number being built
		/// </summary>
		public int Change { get; }

		/// <summary>
		/// Log for this step
		/// </summary>
		public LogId? LogId { get; }

		/// <summary>
		/// The agent type
		/// </summary>
		public PoolId? PoolId { get; }

		/// <summary>
		/// The agent id
		/// </summary>
		public AgentId? AgentId { get; }

		/// <summary>
		/// Outcome of the step, once complete.
		/// </summary>
		public JobStepOutcome? Outcome { get; }

		/// <summary>
		/// Whether this step should update issues
		/// </summary>
		public bool UpdateIssues { get; }

		/// <summary>
		/// Issues ids affecting this job step
		/// </summary>
		public IReadOnlyList<int>? IssueIds { get; }

		/// <summary>
		/// Whether this step is part of a bisection
		/// </summary>
		public BisectTaskId? BisectTaskId { get; }

		/// <summary>
		/// The last change that succeeded. Note that this is only set when the ref is updated; it is not necessarily consistent with steps run later.
		/// </summary>
		public int? LastSuccess { get; }

		/// <summary>
		/// The last change that succeeded, or completed a warning. See <see cref="LastSuccess"/>.
		/// </summary>
		public int? LastWarning { get; }

		/// <summary>
		/// Time taken for the batch containing this batch to start after it became ready
		/// </summary>
		public float BatchWaitTime { get; }

		/// <summary>
		/// Time taken for this batch to initialize
		/// </summary>
		public float BatchInitTime { get; }

		/// <summary>
		/// Time at which the step started.
		/// </summary>
		public DateTime StartTimeUtc { get; }

		/// <summary>
		/// Time at which the step finished.
		/// </summary>
		public DateTime? FinishTimeUtc { get; }
	}
}
