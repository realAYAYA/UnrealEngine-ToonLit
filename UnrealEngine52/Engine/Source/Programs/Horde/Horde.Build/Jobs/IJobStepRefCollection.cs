// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Build.Agents;
using Horde.Build.Agents.Pools;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Logs;
using Horde.Build.Streams;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.Extensions.Logging;

namespace Horde.Build.Jobs
{
	using LogId = ObjectId<ILogFile>;
	using PoolId = StringId<IPool>;
	using StreamId = StringId<IStream>;
	using TemplateId = StringId<ITemplateRef>;

	/// <summary>
	/// Interface for a collection of JobStepRef documents
	/// </summary>
	public interface IJobStepRefCollection
	{
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="id">Unique id for the step being referenced</param>
		/// <param name="jobName">Name of the job</param>
		/// <param name="stepName">Name of the step</param>
		/// <param name="streamId">Unique id for the stream containing the job</param>
		/// <param name="templateId"></param>
		/// <param name="change">The change number being built</param>
		/// <param name="logId">The log file id</param>
		/// <param name="poolId">The pool id</param>
		/// <param name="agentId">The agent id</param>
		/// <param name="outcome">Outcome of this step, if known</param>
		/// <param name="updateIssues">Whether this step ref is included for issue updates</param>
		/// <param name="lastSuccess">The last change that completed with success</param>
		/// <param name="lastWarning">The last change that completed with a warning (or success)</param>
		/// <param name="waitTime">Time taken for the batch containing this step to start</param>
		/// <param name="initTime">Time taken for the batch containing this step to initializer</param>
		/// <param name="jobStartTimeUtc">Start time of the job</param>
		/// <param name="startTimeUtc">Start time</param>
		/// <param name="finishTimeUtc">Finish time for the step, if known</param>
		Task<IJobStepRef> InsertOrReplaceAsync(JobStepRefId id, string jobName, string stepName, StreamId streamId, TemplateId templateId, int change, LogId? logId, PoolId? poolId, AgentId? agentId, JobStepOutcome? outcome, bool updateIssues, int? lastSuccess, int? lastWarning, float waitTime, float initTime, DateTime jobStartTimeUtc, DateTime startTimeUtc, DateTime? finishTimeUtc);

		/// <summary>
		/// Gets the history of a given node
		/// </summary>
		/// <param name="streamId">Unique id for a stream</param>
		/// <param name="templateId"></param>
		/// <param name="nodeName">Name of the node</param>
		/// <param name="change">The current change</param>
		/// <param name="includeFailed">Whether to include failed nodes</param>
		/// <param name="count">Number of results to return</param>
		/// <returns>List of step references</returns>
		Task<List<IJobStepRef>> GetStepsForNodeAsync(StreamId streamId, TemplateId templateId, string nodeName, int? change, bool includeFailed, int count);

		/// <summary>
		/// Gets the previous job that ran a given step
		/// </summary>
		/// <param name="streamId">Id of the stream to search</param>
		/// <param name="templateId">The template id</param>
		/// <param name="nodeName">Name of the step to find</param>
		/// <param name="change">The current changelist number</param>
		/// <param name="outcome">The outcome to filter by or include all outcomes if null</param>
		/// <param name="updateIssues">If true, constrain to steps which update issues</param>		 
		/// <returns>The previous job, or null.</returns>
		Task<IJobStepRef?> GetPrevStepForNodeAsync(StreamId streamId, TemplateId templateId, string nodeName, int change, JobStepOutcome? outcome = null, bool? updateIssues = null);

		/// <summary>
		/// Gets the next job that ran a given step
		/// </summary>
		/// <param name="streamId">Id of the stream to search</param>
		/// <param name="templateId">The template id</param>
		/// <param name="nodeName">Name of the step to find</param>
		/// <param name="change">The current changelist number</param>
		/// <param name="outcome">The outcome to filter by or include all outcomes if null</param>
		/// <param name="updateIssues">If true, constrain to steps which update issues</param>
		/// <returns>The previous job, or null.</returns>
		Task<IJobStepRef?> GetNextStepForNodeAsync(StreamId streamId, TemplateId templateId, string nodeName, int change, JobStepOutcome? outcome = null, bool? updateIssues = null);
	}

	static class JobStepRefCollectionExtensions
	{
		public static async Task UpdateAsync(this IJobStepRefCollection jobStepRefs, IJob job, IJobStepBatch batch, IJobStep step, IGraph graph, ILogger? logger = null)
		{
			if (job.PreflightChange == 0)
			{
				float waitTime = (float)(batch.GetWaitTime() ?? TimeSpan.Zero).TotalSeconds;
				float initTime = (float)(batch.GetInitTime() ?? TimeSpan.Zero).TotalSeconds;

				string nodeName = graph.Groups[batch.GroupIdx].Nodes[step.NodeIdx].Name;
				JobStepOutcome? outcome = step.IsPending() ? (JobStepOutcome?)null : step.Outcome;

				int? lastSuccess = null;
				int? lastWarning = null;
				if (outcome != JobStepOutcome.Success)
				{
					IJobStepRef? prevStep = await jobStepRefs.GetPrevStepForNodeAsync(job.StreamId, job.TemplateId, nodeName, job.Change);
					if (prevStep != null)
					{
						lastSuccess = prevStep.LastSuccess;
						if (outcome != JobStepOutcome.Warnings)
						{
							lastWarning = prevStep.LastWarning;
						}
					}
				}

				if (job.AbortedByUserId != null && outcome == null)
				{
					outcome = JobStepOutcome.Unspecified;
				}

				if (batch.Error != JobStepBatchError.None && outcome == null)
				{
					outcome = JobStepOutcome.Unspecified;
				}
				
				if (logger != null)
				{
					logger.LogInformation("Updating step reference {StepId} for job {JobId}, batch {BatchId}, with outcome {JobStepOutcome}", step.Id, job.Id, batch.Id, outcome);
				}				

				await jobStepRefs.InsertOrReplaceAsync(new JobStepRefId(job.Id, batch.Id, step.Id), job.Name, nodeName, job.StreamId, job.TemplateId, job.Change, step.LogId, batch.PoolId, batch.AgentId, outcome, job.UpdateIssues, lastSuccess, lastWarning, waitTime, initTime, job.CreateTimeUtc, step.StartTimeUtc ?? DateTime.UtcNow, step.FinishTimeUtc);
			}
		}
	}
}
