// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using EpicGames.Horde.Issues;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Logs;
using Horde.Server.Jobs.Graphs;
using MongoDB.Bson;

namespace Horde.Server.Issues
{
	/// <summary>
	/// Identifies a particular changelist and job that contributes to a span
	/// </summary>
	public interface IIssueStep
	{
		/// <summary>
		/// The span that this step belongs to
		/// </summary>
		public ObjectId SpanId { get; }

		/// <summary>
		/// The changelist number
		/// </summary>
		public int Change { get; }

		/// <summary>
		/// Severity of the issue in this step
		/// </summary>
		public IssueSeverity Severity { get; }

		/// <summary>
		/// Name of the job
		/// </summary>
		public string JobName { get; }

		/// <summary>
		/// The unique job id
		/// </summary>
		public JobId JobId { get; }

		/// <summary>
		/// Unique id of the batch within the job
		/// </summary>
		public JobStepBatchId BatchId { get; }

		/// <summary>
		/// Unique id of the step within the job
		/// </summary>
		public JobStepId StepId { get; }

		/// <summary>
		/// Time that the step started
		/// </summary>
		public DateTime StepTime { get; }

		/// <summary>
		/// Annotations for this step
		/// </summary>
		public IReadOnlyNodeAnnotations Annotations { get; }

		/// <summary>
		/// Whether to promote spans including this step by default
		/// </summary>
		public bool PromoteByDefault { get; }

		/// <summary>
		/// The log id for this step
		/// </summary>
		public LogId? LogId { get; }
	}
}
