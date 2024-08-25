// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Security.Claims;
using EpicGames.Core;
using EpicGames.Horde.Acls;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Pools;
using EpicGames.Horde.Agents.Sessions;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Jobs.Bisect;
using EpicGames.Horde.Jobs.Templates;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Streams;
using EpicGames.Horde.Users;
using Horde.Server.Acls;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Jobs.Timing;
using Horde.Server.Server;
using Horde.Server.Streams;
using Horde.Server.Ugs;
using HordeCommon;
using HordeCommon.Rpc.Tasks;
using MongoDB.Bson;
using MongoDB.Driver;

namespace Horde.Server.Jobs
{
	using ReportPlacement = HordeCommon.Rpc.ReportPlacement;

	/// <summary>
	/// Report for a job or jobstep
	/// </summary>
	public interface IReport
	{
		/// <summary>
		/// Name of the report
		/// </summary>
		string Name { get; }

		/// <summary>
		/// Where to render the report
		/// </summary>
		ReportPlacement Placement { get; }

		/// <summary>
		/// The artifact id
		/// </summary>
		ObjectId? ArtifactId { get; }

		/// <summary>
		/// Inline data for the report
		/// </summary>
		string? Content { get; }
	}

	/// <summary>
	/// Implementation of IReport
	/// </summary>
	public class Report : IReport
	{
		/// <inheritdoc/>
		public string Name { get; set; } = String.Empty;

		/// <inheritdoc/>
		public ReportPlacement Placement { get; set; }

		/// <inheritdoc/>
		public ObjectId? ArtifactId { get; set; }

		/// <inheritdoc/>
		public string? Content { get; set; }
	}

	/// <summary>
	/// Embedded jobstep document
	/// </summary>
	public interface IJobStep
	{
		/// <summary>
		/// Unique ID assigned to this jobstep. A new id is generated whenever a jobstep's order is changed.
		/// </summary>
		public JobStepId Id { get; }

		/// <summary>
		/// Index of the node which this jobstep is to execute
		/// </summary>
		public int NodeIdx { get; }

		/// <summary>
		/// Current state of the job step. This is updated automatically when runs complete.
		/// </summary>
		public JobStepState State { get; }

		/// <summary>
		/// Current outcome of the jobstep
		/// </summary>
		public JobStepOutcome Outcome { get; }

		/// <summary>
		/// Error from executing this step
		/// </summary>
		public JobStepError Error { get; }

		/// <summary>
		/// The log id for this step
		/// </summary>
		public LogId? LogId { get; }

		/// <summary>
		/// Unique id for notifications
		/// </summary>
		public ObjectId? NotificationTriggerId { get; }

		/// <summary>
		/// Time at which the batch transitioned to the ready state (UTC).
		/// </summary>
		public DateTime? ReadyTimeUtc { get; }

		/// <summary>
		/// Time at which the batch transitioned to the executing state (UTC).
		/// </summary>
		public DateTime? StartTimeUtc { get; }

		/// <summary>
		/// Time at which the run finished (UTC)
		/// </summary>
		public DateTime? FinishTimeUtc { get; }

		/// <summary>
		/// Override for the priority of this step
		/// </summary>
		public Priority? Priority { get; }

		/// <summary>
		/// If a retry is requested, stores the name of the user that requested it
		/// </summary>
		public UserId? RetriedByUserId { get; }

		/// <summary>
		/// Signal if a step should be aborted
		/// </summary>
		public bool AbortRequested { get; }

		/// <summary>
		/// If an abort is requested, stores the id of the user that requested it
		/// </summary>
		public UserId? AbortedByUserId { get; }

		/// <summary>
		/// List of reports for this step
		/// </summary>
		public IReadOnlyList<IReport>? Reports { get; }

		/// <summary>
		/// Reports for this jobstep.
		/// </summary>
		public Dictionary<string, string>? Properties { get; }
	}

	/// <summary>
	/// Systemic error codes for a job step failing
	/// </summary>
	public enum JobStepError
	{
		/// <summary>
		/// No systemic error
		/// </summary>
		None,

		/// <summary>
		/// Step did not complete in the required amount of time
		/// </summary>
		TimedOut,

		/// <summary>
		/// Step is in is paused state so was skipped
		/// </summary>
		Paused,

		/// <summary>
		/// Step did not complete because the batch exited
		/// </summary>
		Incomplete
	}

	/// <summary>
	/// Extension methods for job steps
	/// </summary>
	public static class JobStepExtensions
	{
		/// <summary>
		/// Determines if a jobstep has failed or is skipped. Can be used to determine whether dependent steps will be able to run.
		/// </summary>
		/// <returns>True if the step is failed or skipped</returns>
		public static bool IsFailedOrSkipped(this IJobStep step)
		{
			return step.State == JobStepState.Skipped || step.Outcome == JobStepOutcome.Failure;
		}

		/// <summary>
		/// Determines if a jobstep is done by checking to see if it is completed, skipped, or aborted.
		/// </summary>
		/// <returns>True if the step is completed, skipped, or aborted</returns>
		public static bool IsPending(this IJobStep step)
		{
			return step.State != JobStepState.Aborted && step.State != JobStepState.Completed && step.State != JobStepState.Skipped;
		}

		/// <summary>
		/// Determine if a step should be timed out
		/// </summary>
		/// <param name="step"></param>
		/// <param name="utcNow"></param>
		/// <returns></returns>
		public static bool HasTimedOut(this IJobStep step, DateTime utcNow)
		{
			if (step.State == JobStepState.Running && step.StartTimeUtc != null)
			{
				TimeSpan elapsed = utcNow - step.StartTimeUtc.Value;
				if (elapsed > TimeSpan.FromHours(24.0))
				{
					return true;
				}
			}
			return false;
		}
	}

	/// <summary>
	/// Stores information about a batch of job steps
	/// </summary>
	public interface IJobStepBatch
	{
		/// <summary>
		/// Unique id for this group
		/// </summary>
		public JobStepBatchId Id { get; }

		/// <summary>
		/// The log file id for this batch
		/// </summary>
		public LogId? LogId { get; }

		/// <summary>
		/// Index of the group being executed
		/// </summary>
		public int GroupIdx { get; }

		/// <summary>
		/// The state of this group
		/// </summary>
		public JobStepBatchState State { get; }

		/// <summary>
		/// Error associated with this group
		/// </summary>
		public JobStepBatchError Error { get; }

		/// <summary>
		/// Steps within this run
		/// </summary>
		public IReadOnlyList<IJobStep> Steps { get; }

		/// <summary>
		/// The pool that this agent was taken from
		/// </summary>
		public PoolId? PoolId { get; }

		/// <summary>
		/// The agent assigned to execute this group
		/// </summary>
		public AgentId? AgentId { get; }

		/// <summary>
		/// The agent session that is executing this group
		/// </summary>
		public SessionId? SessionId { get; }

		/// <summary>
		/// The lease that's executing this group
		/// </summary>
		public LeaseId? LeaseId { get; }

		/// <summary>
		/// The weighted priority of this batch for the scheduler
		/// </summary>
		public int SchedulePriority { get; }

		/// <summary>
		/// Time at which the group became ready (UTC).
		/// </summary>
		public DateTime? ReadyTimeUtc { get; }

		/// <summary>
		/// Time at which the group started (UTC).
		/// </summary>
		public DateTime? StartTimeUtc { get; }

		/// <summary>
		/// Time at which the group finished (UTC)
		/// </summary>
		public DateTime? FinishTimeUtc { get; }
	}

	/// <summary>
	/// Extension methods for IJobStepBatch
	/// </summary>
	public static class JobStepBatchExtensions
	{
		/// <summary>
		/// Attempts to get a step with the given id
		/// </summary>
		/// <param name="batch">The batch to search</param>
		/// <param name="stepId">The step id</param>
		/// <param name="step">On success, receives the step object</param>
		/// <returns>True if the step was found</returns>
		public static bool TryGetStep(this IJobStepBatch batch, JobStepId stepId, [NotNullWhen(true)] out IJobStep? step)
		{
			step = batch.Steps.FirstOrDefault(x => x.Id == stepId);
			return step != null;
		}

		/// <summary>
		/// Determines if new steps can be appended to this batch. We do not allow this after the last step has been completed, because the agent is shutting down.
		/// </summary>
		/// <param name="batch">The batch to search</param>
		/// <returns>True if new steps can be appended to this batch</returns>
		public static bool CanBeAppendedTo(this IJobStepBatch batch)
		{
			return batch.State <= JobStepBatchState.Running;
		}

		/// <summary>
		/// Gets the wait time for this batch
		/// </summary>
		/// <param name="batch">The batch to search</param>
		/// <returns>Wait time for the batch</returns>
		public static TimeSpan? GetWaitTime(this IJobStepBatch batch)
		{
			if (batch.StartTimeUtc == null || batch.ReadyTimeUtc == null)
			{
				return null;
			}
			else
			{
				return batch.StartTimeUtc.Value - batch.ReadyTimeUtc.Value;
			}
		}

		/// <summary>
		/// Gets the initialization time for this batch
		/// </summary>
		/// <param name="batch">The batch to search</param>
		/// <returns>Initialization time for this batch</returns>
		public static TimeSpan? GetInitTime(this IJobStepBatch batch)
		{
			if (batch.StartTimeUtc != null)
			{
				foreach (IJobStep step in batch.Steps)
				{
					if (step.StartTimeUtc != null)
					{
						return step.StartTimeUtc - batch.StartTimeUtc.Value;
					}
				}
			}
			return null;
		}

		/// <summary>
		/// Get the dependencies required for this batch to start, taking run-early nodes into account
		/// </summary>
		/// <param name="batch">The batch to search</param>
		/// <param name="groups">List of node groups</param>
		/// <returns>Set of nodes that must have completed for this batch to start</returns>
		public static HashSet<INode> GetStartDependencies(this IJobStepBatch batch, IReadOnlyList<INodeGroup> groups)
		{
			// Find all the nodes that this group will start with.
			List<INode> nodes = batch.Steps.ConvertAll(x => groups[batch.GroupIdx].Nodes[x.NodeIdx]);
			if (nodes.Any(x => x.RunEarly))
			{
				nodes.RemoveAll(x => !x.RunEarly);
			}

			// Find all their dependencies
			HashSet<INode> dependencies = new HashSet<INode>();
			foreach (INode node in nodes)
			{
				dependencies.UnionWith(node.InputDependencies.Select(x => groups[x.GroupIdx].Nodes[x.NodeIdx]));
				dependencies.UnionWith(node.OrderDependencies.Select(x => groups[x.GroupIdx].Nodes[x.NodeIdx]));
			}

			// Exclude all the dependencies within the same group
			dependencies.ExceptWith(groups[batch.GroupIdx].Nodes);
			return dependencies;
		}
	}

	/// <summary>
	/// Cumulative timing information to reach a certain point in a job
	/// </summary>
	public class TimingInfo
	{
		/// <summary>
		/// Wait time on the critical path
		/// </summary>
		public TimeSpan? TotalWaitTime { get; set; }

		/// <summary>
		/// Sync time on the critical path
		/// </summary>
		public TimeSpan? TotalInitTime { get; set; }

		/// <summary>
		/// Duration to this point
		/// </summary>
		public TimeSpan? TotalTimeToComplete { get; set; }

		/// <summary>
		/// Average wait time to this point
		/// </summary>
		public TimeSpan? AverageTotalWaitTime { get; set; }

		/// <summary>
		/// Average sync time to this point
		/// </summary>
		public TimeSpan? AverageTotalInitTime { get; set; }

		/// <summary>
		/// Average duration to this point
		/// </summary>
		public TimeSpan? AverageTotalTimeToComplete { get; set; }

		/// <summary>
		/// Individual step timing information
		/// </summary>
		public IJobStepTiming? StepTiming { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		public TimingInfo()
		{
			TotalWaitTime = TimeSpan.Zero;
			TotalInitTime = TimeSpan.Zero;
			TotalTimeToComplete = TimeSpan.Zero;

			AverageTotalWaitTime = TimeSpan.Zero;
			AverageTotalInitTime = TimeSpan.Zero;
			AverageTotalTimeToComplete = TimeSpan.Zero;
		}

		/// <summary>
		/// Copy constructor
		/// </summary>
		/// <param name="other">The timing info object to copy from</param>
		public TimingInfo(TimingInfo other)
		{
			TotalWaitTime = other.TotalWaitTime;
			TotalInitTime = other.TotalInitTime;
			TotalTimeToComplete = other.TotalTimeToComplete;

			AverageTotalWaitTime = other.AverageTotalWaitTime;
			AverageTotalInitTime = other.AverageTotalInitTime;
			AverageTotalTimeToComplete = other.AverageTotalTimeToComplete;
		}

		/// <summary>
		/// Modifies this timing to wait for another timing
		/// </summary>
		/// <param name="other">The other node to wait for</param>
		public void WaitFor(TimingInfo other)
		{
			if (TotalTimeToComplete != null)
			{
				if (other.TotalTimeToComplete == null || other.TotalTimeToComplete.Value > TotalTimeToComplete.Value)
				{
					TotalInitTime = other.TotalInitTime;
					TotalWaitTime = other.TotalWaitTime;
					TotalTimeToComplete = other.TotalTimeToComplete;
				}
			}

			if (AverageTotalTimeToComplete != null)
			{
				if (other.AverageTotalTimeToComplete == null || other.AverageTotalTimeToComplete.Value > AverageTotalTimeToComplete.Value)
				{
					AverageTotalInitTime = other.AverageTotalInitTime;
					AverageTotalWaitTime = other.AverageTotalWaitTime;
					AverageTotalTimeToComplete = other.AverageTotalTimeToComplete;
				}
			}
		}

		/// <summary>
		/// Waits for all the given timing info objects to complete
		/// </summary>
		/// <param name="others">Other timing info objects to wait for</param>
		public void WaitForAll(IEnumerable<TimingInfo> others)
		{
			foreach (TimingInfo other in others)
			{
				WaitFor(other);
			}
		}

		/// <summary>
		/// Constructs a new TimingInfo object which represents the last TimingInfo to finish
		/// </summary>
		/// <param name="others">TimingInfo objects to wait for</param>
		/// <returns>New TimingInfo instance</returns>
		public static TimingInfo Max(IEnumerable<TimingInfo> others)
		{
			TimingInfo timingInfo = new TimingInfo();
			timingInfo.WaitForAll(others);
			return timingInfo;
		}
	}

	/// <summary>
	/// Information about a chained job trigger
	/// </summary>
	public interface IChainedJob
	{
		/// <summary>
		/// The target to monitor
		/// </summary>
		public string Target { get; }

		/// <summary>
		/// The template to trigger on success
		/// </summary>
		public TemplateId TemplateRefId { get; }

		/// <summary>
		/// The triggered job id
		/// </summary>
		public JobId? JobId { get; }

		/// <summary>
		/// Whether to run the latest change, or default change for the template, when starting the new job. Uses same change as the triggering job by default.
		/// </summary>
		public bool UseDefaultChangeForTemplate { get; }
	}

	/// <summary>
	/// Document describing a job
	/// </summary>
	public interface IJob
	{
		/// <summary>
		/// Job argument indicating a target that should be built
		/// </summary>
		public const string TargetArgumentPrefix = "-Target=";

		/// <summary>
		/// Name of the node which parses the buildgraph script
		/// </summary>
		public const string SetupNodeName = "Setup Build";

		/// <summary>
		/// Identifier for the job. Randomly generated.
		/// </summary>
		public JobId Id { get; }

		/// <summary>
		/// The stream that this job belongs to
		/// </summary>
		public StreamId StreamId { get; }

		/// <summary>
		/// The template ref id
		/// </summary>
		public TemplateId TemplateId { get; }

		/// <summary>
		/// The template that this job was created from
		/// </summary>
		public ContentHash? TemplateHash { get; }

		/// <summary>
		/// Hash of the graph definition
		/// </summary>
		public ContentHash GraphHash { get; }

		/// <summary>
		/// Id of the user that started this job
		/// </summary>
		public UserId? StartedByUserId { get; }

		/// <summary>
		/// Id of the user that aborted this job. Set to null if the job is not aborted.
		/// </summary>
		public UserId? AbortedByUserId { get; }

		/// <summary>
		/// Identifier of the bisect task that started this job
		/// </summary>
		public BisectTaskId? StartedByBisectTaskId { get; }

		/// <summary>
		/// Name of the job.
		/// </summary>
		public string Name { get; }

		/// <summary>
		/// The changelist number to build
		/// </summary>
		public int Change { get; }

		/// <summary>
		/// The code changelist number for this build
		/// </summary>
		public int CodeChange { get; }

		/// <summary>
		/// The preflight changelist number
		/// </summary>
		public int PreflightChange { get; }

		/// <summary>
		/// The cloned preflight changelist number (if the prefight change is duplicated via p4 reshelve)
		/// </summary>
		public int ClonedPreflightChange { get; }

		/// <summary>
		/// Description for the shelved change if running a preflight
		/// </summary>
		public string? PreflightDescription { get; }

		/// <summary>
		/// Priority of this job
		/// </summary>
		public Priority Priority { get; }

		/// <summary>
		/// For preflights, submit the change if the job is successful
		/// </summary>
		public bool AutoSubmit { get; }

		/// <summary>
		/// The submitted changelist number
		/// </summary>
		public int? AutoSubmitChange { get; }

		/// <summary>
		/// Message produced by trying to auto-submit the change
		/// </summary>
		public string? AutoSubmitMessage { get; }

		/// <summary>
		/// Whether to update issues based on the outcome of this job
		/// </summary>
		public bool UpdateIssues { get; }

		/// <summary>
		/// Whether to promote issues by default based on the outcome of this job
		/// </summary>
		public bool PromoteIssuesByDefault { get; }

		/// <summary>
		/// Time that the job was created (in UTC)
		/// </summary>
		public DateTime CreateTimeUtc { get; }

		/// <summary>
		/// Options for executing the job
		/// </summary>
		public JobOptions? JobOptions { get; }

		/// <summary>
		/// Claims inherited from the user that started this job
		/// </summary>
		public IReadOnlyList<AclClaimConfig> Claims { get; }

		/// <summary>
		/// Largest value of the CombinedPriority value for batches in the ready state.
		/// </summary>
		public int SchedulePriority { get; }

		/// <summary>
		/// Array of jobstep runs
		/// </summary>
		public IReadOnlyList<IJobStepBatch> Batches { get; }

		/// <summary>
		/// Optional user-defined properties for this job
		/// </summary>
		public IReadOnlyList<string> Arguments { get; }

		/// <summary>
		/// Environment variables for the job
		/// </summary>
		public IReadOnlyDictionary<string, string> Environment { get; }

		/// <summary>
		/// Issues associated with this job
		/// </summary>
		public IReadOnlyList<int> Issues { get; }

		/// <summary>
		/// Unique id for notifications
		/// </summary>
		public ObjectId? NotificationTriggerId { get; }

		/// <summary>
		/// Whether to show badges in UGS for this job
		/// </summary>
		public bool ShowUgsBadges { get; }

		/// <summary>
		/// Whether to show alerts in UGS for this job
		/// </summary>
		public bool ShowUgsAlerts { get; }

		/// <summary>
		/// Notification channel for this job.
		/// </summary>
		public string? NotificationChannel { get; }

		/// <summary>
		/// Notification channel filter for this job.
		/// </summary>
		public string? NotificationChannelFilter { get; }

		/// <summary>
		/// Mapping of label ids to notification trigger ids for notifications
		/// </summary>
		public IReadOnlyDictionary<int, ObjectId> LabelIdxToTriggerId { get; }

		/// <summary>
		/// List of reports for this step
		/// </summary>
		public IReadOnlyList<IReport>? Reports { get; }

		/// <summary>
		/// List of downstream job triggers
		/// </summary>
		public IReadOnlyList<IChainedJob> ChainedJobs { get; }

		/// <summary>
		/// Next id for batches or groups
		/// </summary>
		public SubResourceId NextSubResourceId { get; }

		/// <summary>
		/// The last update time
		/// </summary>
		public DateTime UpdateTimeUtc { get; }

		/// <summary>
		/// Update counter for this document. Any updates should compare-and-swap based on the value of this counter, or increment it in the case of server-side updates.
		/// </summary>
		public int UpdateIndex { get; }
	}

	/// <summary>
	/// Extension methods for jobs
	/// </summary>
	public static class JobExtensions
	{
		/// <summary>
		/// Determines if the user is authorized to perform an action on a particular stream
		/// </summary>
		/// <param name="globalConfig">Current global config instance</param>
		/// <param name="job">Job to authorize for</param>
		/// <param name="action">The action being performed</param>
		/// <param name="user">The principal to authorize</param>
		/// <returns>True if the action is authorized</returns>
		public static bool Authorize(this GlobalConfig globalConfig, IJob? job, AclAction action, ClaimsPrincipal user)
		{
			if (job != null && globalConfig.TryGetStream(job.StreamId, out StreamConfig? streamConfig))
			{
				return streamConfig.Authorize(action, user);
			}
			else
			{
				return globalConfig.Authorize(action, user);
			}
		}

		/// <summary>
		/// Attempts to get a step with a given ID
		/// </summary>
		/// <param name="job">The job document</param>
		/// <param name="stepId">The step id</param>
		/// <param name="step">On success, receives the step object</param>
		/// <returns>True if the step was found</returns>
		public static bool TryGetStep(this IJob job, JobStepId stepId, [NotNullWhen(true)] out IJobStep? step)
		{
			foreach (IJobStepBatch batch in job.Batches)
			{
				if (batch.TryGetStep(stepId, out step))
				{
					return true;
				}
			}

			step = null;
			return false;
		}

		/// <summary>
		/// Attempts to get a step with a given ID
		/// </summary>
		/// <param name="job">The job document</param>
		/// <param name="stepId">The step id</param>
		/// <param name="batch">On success returns the batch containing the step</param>
		/// <param name="step">On success, receives the step object</param>
		/// <returns>True if the step was found</returns>
		public static bool TryGetStep(this IJob job, JobStepId stepId, [NotNullWhen(true)] out IJobStepBatch? batch, [NotNullWhen(true)] out IJobStep? step)
		{
			foreach (IJobStepBatch currentBatch in job.Batches)
			{
				if (currentBatch.TryGetStep(stepId, out IJobStep? currentStep))
				{
					batch = currentBatch;
					step = currentStep;
					return true;
				}
			}

			batch = null;
			step = null;
			return false;
		}

		/// <summary>
		/// Gets the current job state
		/// </summary>
		/// <param name="job">The job document</param>
		/// <returns>Job state</returns>
		public static JobState GetState(this IJob job)
		{
			bool waiting = false;
			foreach (IJobStepBatch batch in job.Batches)
			{
				foreach (IJobStep step in batch.Steps)
				{
					if (step.State == JobStepState.Running)
					{
						return JobState.Running;
					}
					else if (step.State == JobStepState.Ready || step.State == JobStepState.Waiting)
					{
						if (batch.State == JobStepBatchState.Starting || batch.State == JobStepBatchState.Running)
						{
							return JobState.Running;
						}
						else
						{
							waiting = true;
						}
					}
				}
			}
			return waiting ? JobState.Waiting : JobState.Complete;
		}

		/// <summary>
		/// Gets the outcome for a particular named target. May be an aggregate or node name.
		/// </summary>
		/// <param name="job">The job to check</param>
		/// <returns>The step outcome</returns>
		public static (JobStepState, JobStepOutcome) GetTargetState(this IJob job)
		{
			IReadOnlyDictionary<NodeRef, IJobStep> nodeToStep = GetStepForNodeMap(job);
			return GetTargetState(nodeToStep.Values);
		}

		/// <summary>
		/// Gets the outcome for a particular named target. May be an aggregate or node name.
		/// </summary>
		/// <param name="job">The job to check</param>
		/// <param name="graph">Graph for the job</param>
		/// <param name="target">Target to find an outcome for</param>
		/// <returns>The step outcome</returns>
		public static (JobStepState, JobStepOutcome)? GetTargetState(this IJob job, IGraph graph, string? target)
		{
			if (target == null)
			{
				return GetTargetState(job);
			}

			NodeRef nodeRef;
			if (graph.TryFindNode(target, out nodeRef))
			{
				IJobStep? step;
				if (job.TryGetStepForNode(nodeRef, out step))
				{
					return (step.State, step.Outcome);
				}
				else
				{
					return null;
				}
			}

			IAggregate? aggregate;
			if (graph.TryFindAggregate(target, out aggregate))
			{
				IReadOnlyDictionary<NodeRef, IJobStep> stepForNode = GetStepForNodeMap(job);

				List<IJobStep> steps = new List<IJobStep>();
				foreach (NodeRef aggregateNodeRef in aggregate.Nodes)
				{
					IJobStep? step;
					if (!stepForNode.TryGetValue(aggregateNodeRef, out step))
					{
						return null;
					}
					steps.Add(step);
				}

				return GetTargetState(steps);
			}

			return null;
		}

		/// <summary>
		/// Gets the outcome for a particular named target. May be an aggregate or node name.
		/// </summary>
		/// <param name="steps">Steps to include</param>
		/// <returns>The step outcome</returns>
		public static (JobStepState, JobStepOutcome) GetTargetState(IEnumerable<IJobStep> steps)
		{
			bool anySkipped = false;
			bool anyWarnings = false;
			bool anyFailed = false;
			bool anyPending = false;
			foreach (IJobStep step in steps)
			{
				anyPending |= step.IsPending();
				anySkipped |= step.State == JobStepState.Aborted || step.State == JobStepState.Skipped;
				anyFailed |= (step.Outcome == JobStepOutcome.Failure);
				anyWarnings |= (step.Outcome == JobStepOutcome.Warnings);
			}

			JobStepState newState = anyPending ? JobStepState.Running : JobStepState.Completed;
			JobStepOutcome newOutcome = anyFailed ? JobStepOutcome.Failure : anyWarnings ? JobStepOutcome.Warnings : anySkipped ? JobStepOutcome.Unspecified : JobStepOutcome.Success;
			return (newState, newOutcome);
		}

		/// <summary>
		/// Gets the outcome for a particular named target. May be an aggregate or node name.
		/// </summary>
		/// <param name="job">The job to check</param>
		/// <param name="graph">Graph for the job</param>
		/// <param name="target">Target to find an outcome for</param>
		/// <returns>The step outcome</returns>
		public static JobStepOutcome GetTargetOutcome(this IJob job, IGraph graph, string target)
		{
			NodeRef nodeRef;
			if (graph.TryFindNode(target, out nodeRef))
			{
				IJobStep? step;
				if (job.TryGetStepForNode(nodeRef, out step))
				{
					return step.Outcome;
				}
				else
				{
					return JobStepOutcome.Unspecified;
				}
			}

			IAggregate? aggregate;
			if (graph.TryFindAggregate(target, out aggregate))
			{
				IReadOnlyDictionary<NodeRef, IJobStep> stepForNode = GetStepForNodeMap(job);

				bool warnings = false;
				foreach (NodeRef aggregateNodeRef in aggregate.Nodes)
				{
					IJobStep? step;
					if (!stepForNode.TryGetValue(aggregateNodeRef, out step))
					{
						return JobStepOutcome.Unspecified;
					}
					if (step.Outcome == JobStepOutcome.Failure)
					{
						return JobStepOutcome.Failure;
					}
					warnings |= (step.Outcome == JobStepOutcome.Warnings);
				}
				return warnings ? JobStepOutcome.Warnings : JobStepOutcome.Success;
			}

			return JobStepOutcome.Unspecified;
		}

		/// <summary>
		/// Gets the job step for a particular node
		/// </summary>
		/// <param name="job">The job to search</param>
		/// <param name="nodeRef">The node ref</param>
		/// <param name="jobStep">Receives the jobstep on success</param>
		/// <returns>True if the jobstep was founds</returns>
		public static bool TryGetStepForNode(this IJob job, NodeRef nodeRef, [NotNullWhen(true)] out IJobStep? jobStep)
		{
			jobStep = null;
			foreach (IJobStepBatch batch in job.Batches)
			{
				if (batch.GroupIdx == nodeRef.GroupIdx)
				{
					foreach (IJobStep batchStep in batch.Steps)
					{
						if (batchStep.NodeIdx == nodeRef.NodeIdx)
						{
							jobStep = batchStep;
						}
					}
				}
			}
			return jobStep != null;
		}

		/// <summary>
		/// Gets a dictionary that maps <see cref="NodeRef"/> objects to their associated
		/// <see cref="IJobStep"/> objects on a <see cref="IJob"/>.
		/// </summary>
		/// <param name="job">The job document</param>
		/// <returns>Map of <see cref="NodeRef"/> to <see cref="IJobStep"/></returns>
		public static IReadOnlyDictionary<NodeRef, IJobStep> GetStepForNodeMap(this IJob job)
		{
			Dictionary<NodeRef, IJobStep> stepForNode = new Dictionary<NodeRef, IJobStep>();
			foreach (IJobStepBatch batch in job.Batches)
			{
				foreach (IJobStep batchStep in batch.Steps)
				{
					NodeRef batchNodeRef = new NodeRef(batch.GroupIdx, batchStep.NodeIdx);
					stepForNode[batchNodeRef] = batchStep;
				}
			}
			return stepForNode;
		}

		/// <summary>
		/// Find the latest step executing the given node
		/// </summary>
		/// <param name="job">The job being run</param>
		/// <param name="nodeRef">Node to find</param>
		/// <returns>The retried step information</returns>
		public static JobStepRefId? FindLatestStepForNode(this IJob job, NodeRef nodeRef)
		{
			for (int batchIdx = job.Batches.Count - 1; batchIdx >= 0; batchIdx--)
			{
				IJobStepBatch batch = job.Batches[batchIdx];
				if (batch.GroupIdx == nodeRef.GroupIdx)
				{
					for (int stepIdx = batch.Steps.Count - 1; stepIdx >= 0; stepIdx--)
					{
						IJobStep step = batch.Steps[stepIdx];
						if (step.NodeIdx == nodeRef.NodeIdx)
						{
							return new JobStepRefId(job.Id, batch.Id, step.Id);
						}
					}
				}
			}
			return null;
		}

		/// <summary>
		/// Gets the estimated timing info for all nodes in the job
		/// </summary>
		/// <param name="job">The job document</param>
		/// <param name="graph">Graph for this job</param>
		/// <param name="jobTiming">Job timing information</param>
		/// <returns>Map of node to expected timing info</returns>
		public static Dictionary<INode, TimingInfo> GetTimingInfo(this IJob job, IGraph graph, IJobTiming jobTiming)
		{
#pragma warning disable IDE0054 // Use compound assignment
			TimeSpan currentTime = DateTime.UtcNow - job.CreateTimeUtc;

			Dictionary<INode, TimingInfo> nodeToTimingInfo = graph.Groups.SelectMany(x => x.Nodes).ToDictionary(x => x, x => new TimingInfo());
			foreach (IJobStepBatch batch in job.Batches)
			{
				INodeGroup group = graph.Groups[batch.GroupIdx];

				// Step through the batch, keeping track of the time that things finish.
				TimingInfo timingInfo = new TimingInfo();

				// Wait for the dependencies for the batch to start
				HashSet<INode> dependencyNodes = batch.GetStartDependencies(graph.Groups);
				timingInfo.WaitForAll(dependencyNodes.Select(x => nodeToTimingInfo[x]));

				// If the batch has actually started, correct the expected time to use this instead
				if (batch.StartTimeUtc != null)
				{
					timingInfo.TotalTimeToComplete = batch.StartTimeUtc - job.CreateTimeUtc;
				}

				// Get the average times for this batch
				TimeSpan? averageWaitTime = GetAverageWaitTime(graph, batch, jobTiming);
				TimeSpan? averageInitTime = GetAverageInitTime(graph, batch, jobTiming);

				// Update the wait times and initialization times along this path
				timingInfo.TotalWaitTime = timingInfo.TotalWaitTime + (batch.GetWaitTime() ?? averageWaitTime);
				timingInfo.TotalInitTime = timingInfo.TotalInitTime + (batch.GetInitTime() ?? averageInitTime);

				// Update the average wait and initialization times too
				timingInfo.AverageTotalWaitTime = timingInfo.AverageTotalWaitTime + averageWaitTime;
				timingInfo.AverageTotalInitTime = timingInfo.AverageTotalInitTime + averageInitTime;

				// Step through the batch, updating the expected times as we go
				foreach (IJobStep step in batch.Steps)
				{
					INode node = group.Nodes[step.NodeIdx];

					// Get the timing for this step
					IJobStepTiming? stepTimingInfo;
					jobTiming.TryGetStepTiming(node.Name, out stepTimingInfo);

					// If the step has already started, update the actual time to reach this point
					if (step.StartTimeUtc != null)
					{
						timingInfo.TotalTimeToComplete = step.StartTimeUtc.Value - job.CreateTimeUtc;
					}

					// If the step hasn't started yet, make sure the start time is later than the current time
					if (step.StartTimeUtc == null && currentTime > timingInfo.TotalTimeToComplete)
					{
						timingInfo.TotalTimeToComplete = currentTime;
					}

					// Wait for all the node dependencies to complete
					timingInfo.WaitForAll(graph.GetDependencies(node).Select(x => nodeToTimingInfo[x]));

					// If the step has actually finished, correct the time to use that instead
					if (step.FinishTimeUtc != null)
					{
						timingInfo.TotalTimeToComplete = step.FinishTimeUtc.Value - job.CreateTimeUtc;
					}
					else
					{
						timingInfo.TotalTimeToComplete = timingInfo.TotalTimeToComplete + NullableTimeSpanFromSeconds(stepTimingInfo?.AverageDuration);
					}

					// If the step hasn't finished yet, make sure the start time is later than the current time
					if (step.FinishTimeUtc == null && currentTime > timingInfo.TotalTimeToComplete)
					{
						timingInfo.TotalTimeToComplete = currentTime;
					}

					// Update the average time to complete
					timingInfo.AverageTotalTimeToComplete = timingInfo.AverageTotalTimeToComplete + NullableTimeSpanFromSeconds(stepTimingInfo?.AverageDuration);

					// Add it to the lookup
					TimingInfo nodeTimingInfo = new TimingInfo(timingInfo);
					nodeTimingInfo.StepTiming = stepTimingInfo;
					nodeToTimingInfo[node] = nodeTimingInfo;
				}
			}
			return nodeToTimingInfo;
#pragma warning restore IDE0054 // Use compound assignment
		}

		/// <summary>
		/// Gets the average wait time for this batch
		/// </summary>
		/// <param name="graph">Graph for the job</param>
		/// <param name="batch">The batch to get timing info for</param>
		/// <param name="jobTiming">The job timing information</param>
		/// <returns>Wait time for the batch</returns>
		public static TimeSpan? GetAverageWaitTime(IGraph graph, IJobStepBatch batch, IJobTiming jobTiming)
		{
			TimeSpan? waitTime = null;
			foreach (IJobStep step in batch.Steps)
			{
				INode node = graph.Groups[batch.GroupIdx].Nodes[step.NodeIdx];
				if (jobTiming.TryGetStepTiming(node.Name, out IJobStepTiming? timingInfo))
				{
					if (timingInfo.AverageWaitTime != null)
					{
						TimeSpan stepWaitTime = TimeSpan.FromSeconds(timingInfo.AverageWaitTime.Value);
						if (waitTime == null || stepWaitTime > waitTime.Value)
						{
							waitTime = stepWaitTime;
						}
					}
				}
			}
			return waitTime;
		}

		/// <summary>
		/// Gets the average initialization time for this batch
		/// </summary>
		/// <param name="graph">Graph for the job</param>
		/// <param name="batch">The batch to get timing info for</param>
		/// <param name="jobTiming">The job timing information</param>
		/// <returns>Initialization time for this batch</returns>
		public static TimeSpan? GetAverageInitTime(IGraph graph, IJobStepBatch batch, IJobTiming jobTiming)
		{
			TimeSpan? initTime = null;
			foreach (IJobStep step in batch.Steps)
			{
				INode node = graph.Groups[batch.GroupIdx].Nodes[step.NodeIdx];
				if (jobTiming.TryGetStepTiming(node.Name, out IJobStepTiming? timingInfo))
				{
					if (timingInfo.AverageInitTime != null)
					{
						TimeSpan stepInitTime = TimeSpan.FromSeconds(timingInfo.AverageInitTime.Value);
						if (initTime == null || stepInitTime > initTime.Value)
						{
							initTime = stepInitTime;
						}
					}
				}
			}
			return initTime;
		}

		/// <summary>
		/// Creates a nullable timespan from a nullable number of seconds
		/// </summary>
		/// <param name="seconds">The number of seconds to construct from</param>
		/// <returns>TimeSpan object</returns>
		static TimeSpan? NullableTimeSpanFromSeconds(float? seconds)
		{
			if (seconds == null)
			{
				return null;
			}
			else
			{
				return TimeSpan.FromSeconds(seconds.Value);
			}
		}

		/// <summary>
		/// Attempts to get a batch with the given id
		/// </summary>
		/// <param name="job">The job document</param>
		/// <param name="batchId">The batch id</param>
		/// <param name="batch">On success, receives the batch object</param>
		/// <returns>True if the batch was found</returns>
		public static bool TryGetBatch(this IJob job, JobStepBatchId batchId, [NotNullWhen(true)] out IJobStepBatch? batch)
		{
			batch = job.Batches.FirstOrDefault(x => x.Id == batchId);
			return batch != null;
		}

		/// <summary>
		/// Attempts to get a batch with the given id
		/// </summary>
		/// <param name="job">The job document</param>
		/// <param name="batchId">The batch id</param>
		/// <param name="stepId">The step id</param>
		/// <param name="step">On success, receives the step object</param>
		/// <returns>True if the batch was found</returns>
		public static bool TryGetStep(this IJob job, JobStepBatchId batchId, JobStepId stepId, [NotNullWhen(true)] out IJobStep? step)
		{
			IJobStepBatch? batch;
			if (!TryGetBatch(job, batchId, out batch))
			{
				step = null;
				return false;
			}
			return batch.TryGetStep(stepId, out step);
		}

		/// <summary>
		/// Finds the set of nodes affected by a label
		/// </summary>
		/// <param name="job">The job document</param>
		/// <param name="graph">Graph definition for the job</param>
		/// <param name="labelIdx">Index of the label. -1 or Graph.Labels.Count are treated as referring to the default lable.</param>
		/// <returns>Set of nodes affected by the given label</returns>
		public static HashSet<NodeRef> GetNodesForLabel(this IJob job, IGraph graph, int labelIdx)
		{
			if (labelIdx != -1 && labelIdx != graph.Labels.Count)
			{
				// Return all the nodes included by the label
				return new HashSet<NodeRef>(graph.Labels[labelIdx].IncludedNodes);
			}
			else
			{
				// Set of nodes which are not covered by an existing label, initially containing everything
				HashSet<NodeRef> unlabeledNodes = new HashSet<NodeRef>();
				for (int groupIdx = 0; groupIdx < graph.Groups.Count; groupIdx++)
				{
					INodeGroup group = graph.Groups[groupIdx];
					for (int nodeIdx = 0; nodeIdx < group.Nodes.Count; nodeIdx++)
					{
						unlabeledNodes.Add(new NodeRef(groupIdx, nodeIdx));
					}
				}

				// Remove all the nodes that are part of an active label
				IReadOnlyDictionary<NodeRef, IJobStep> stepForNode = job.GetStepForNodeMap();
				foreach (ILabel label in graph.Labels)
				{
					if (label.RequiredNodes.Any(x => stepForNode.ContainsKey(x)))
					{
						unlabeledNodes.ExceptWith(label.IncludedNodes);
					}
				}
				return unlabeledNodes;
			}
		}

		/// <summary>
		/// Create a list of aggregate responses, combining the graph definitions with the state of the job
		/// </summary>
		/// <param name="job">The job document</param>
		/// <param name="graph">Graph definition for the job</param>
		/// <param name="responses">List to receive all the responses</param>
		/// <returns>The default label state</returns>
		public static GetDefaultLabelStateResponse? GetLabelStateResponses(this IJob job, IGraph graph, List<GetLabelStateResponse> responses)
		{
			// Create a lookup from noderef to step information
			IReadOnlyDictionary<NodeRef, IJobStep> stepForNode = job.GetStepForNodeMap();

			// Set of nodes which are not covered by an existing label, initially containing everything
			HashSet<NodeRef> unlabeledNodes = new HashSet<NodeRef>();
			for (int groupIdx = 0; groupIdx < graph.Groups.Count; groupIdx++)
			{
				INodeGroup group = graph.Groups[groupIdx];
				for (int nodeIdx = 0; nodeIdx < group.Nodes.Count; nodeIdx++)
				{
					unlabeledNodes.Add(new NodeRef(groupIdx, nodeIdx));
				}
			}

			// Create the responses
			foreach (ILabel label in graph.Labels)
			{
				// Refresh the state for this label
				LabelState newState = LabelState.Unspecified;
				foreach (NodeRef requiredNodeRef in label.RequiredNodes)
				{
					if (stepForNode.ContainsKey(requiredNodeRef))
					{
						newState = LabelState.Complete;
						break;
					}
				}

				// Refresh the outcome
				LabelOutcome newOutcome = LabelOutcome.Success;
				if (newState == LabelState.Complete)
				{
					GetLabelState(label.IncludedNodes, stepForNode, out newState, out newOutcome);
					unlabeledNodes.ExceptWith(label.IncludedNodes);
				}

				// Create the response
				responses.Add(new GetLabelStateResponse(newState, newOutcome));
			}

			// Remove all the nodes that don't have a step
			unlabeledNodes.RemoveWhere(x => !stepForNode.ContainsKey(x));

			// Remove successful "setup build" nodes from the list
			if (graph.Groups.Count > 1 && graph.Groups[0].Nodes.Count > 0)
			{
				INode node = graph.Groups[0].Nodes[0];
				if (node.Name == IJob.SetupNodeName)
				{
					NodeRef nodeRef = new NodeRef(0, 0);
					if (unlabeledNodes.Contains(nodeRef))
					{
						IJobStep step = stepForNode[nodeRef];
						if (step.State == JobStepState.Completed && step.Outcome == JobStepOutcome.Success && responses.Count > 0)
						{
							unlabeledNodes.Remove(nodeRef);
						}
					}
				}
			}

			// Add a response for everything not included elsewhere.
			GetLabelState(unlabeledNodes, stepForNode, out LabelState otherState, out LabelOutcome otherOutcome);
			return new GetDefaultLabelStateResponse(otherState, otherOutcome, unlabeledNodes.Select(x => graph.GetNode(x).Name).ToList());
		}

		/// <summary>
		/// Get the states of all labels for this job
		/// </summary>
		/// <param name="job">The job to get states for</param>
		/// <param name="graph">The graph for this job</param>
		/// <returns>Collection of label states by label index</returns>
		public static IReadOnlyList<(LabelState, LabelOutcome)> GetLabelStates(this IJob job, IGraph graph)
		{
			IReadOnlyDictionary<NodeRef, IJobStep> stepForNodeRef = job.GetStepForNodeMap();

			List<(LabelState, LabelOutcome)> states = new List<(LabelState, LabelOutcome)>();
			for (int idx = 0; idx < graph.Labels.Count; idx++)
			{
				ILabel label = graph.Labels[idx];

				// Default the label to the unspecified state
				LabelState newState = LabelState.Unspecified;
				LabelOutcome newOutcome = LabelOutcome.Unspecified;

				// Check if the label should be included
				if (label.RequiredNodes.Any(x => stepForNodeRef.ContainsKey(x)))
				{
					// Combine the state of the steps contributing towards this label
					bool anySkipped = false;
					bool anyWarnings = false;
					bool anyFailed = false;
					bool anyPending = false;
					foreach (NodeRef includedNode in label.IncludedNodes)
					{
						IJobStep? step;
						if (stepForNodeRef.TryGetValue(includedNode, out step))
						{
							anyPending |= step.IsPending();
							anySkipped |= step.State == JobStepState.Aborted || step.State == JobStepState.Skipped;
							anyFailed |= (step.Outcome == JobStepOutcome.Failure);
							anyWarnings |= (step.Outcome == JobStepOutcome.Warnings);
						}
					}

					// Figure out the overall label state
					newState = anyPending ? LabelState.Running : LabelState.Complete;
					newOutcome = anyFailed ? LabelOutcome.Failure : anyWarnings ? LabelOutcome.Warnings : anySkipped ? LabelOutcome.Unspecified : LabelOutcome.Success;
				}

				states.Add((newState, newOutcome));
			}
			return states;
		}

		/// <summary>
		/// Get the states of all UGS badges for this job
		/// </summary>
		/// <param name="job">The job to get states for</param>
		/// <param name="graph">The graph for this job</param>
		/// <returns>List of badge states</returns>
		public static Dictionary<int, UgsBadgeState> GetUgsBadgeStates(this IJob job, IGraph graph)
		{
			IReadOnlyList<(LabelState, LabelOutcome)> labelStates = GetLabelStates(job, graph);
			return job.GetUgsBadgeStates(graph, labelStates);
		}

		/// <summary>
		/// Get the states of all UGS badges for this job
		/// </summary>
		/// <param name="job">The job to get states for</param>
		/// <param name="graph">The graph for this job</param>
		/// <param name="labelStates">The existing label states to get the UGS badge states from</param>
		/// <returns>List of badge states</returns>
#pragma warning disable IDE0060 // Remove unused parameter
		public static Dictionary<int, UgsBadgeState> GetUgsBadgeStates(this IJob job, IGraph graph, IReadOnlyList<(LabelState, LabelOutcome)> labelStates)
#pragma warning restore IDE0060 // Remove unused parameter
		{
			Dictionary<int, UgsBadgeState> ugsBadgeStates = new Dictionary<int, UgsBadgeState>();
			for (int labelIdx = 0; labelIdx < labelStates.Count; ++labelIdx)
			{
				if (graph.Labels[labelIdx].UgsName == null)
				{
					continue;
				}

				(LabelState state, LabelOutcome outcome) = labelStates[labelIdx];
				switch (state)
				{
					case LabelState.Complete:
						{
							switch (outcome)
							{
								case LabelOutcome.Success:
									{
										ugsBadgeStates.Add(labelIdx, UgsBadgeState.Success);
										break;
									}

								case LabelOutcome.Warnings:
									{
										ugsBadgeStates.Add(labelIdx, UgsBadgeState.Warning);
										break;
									}

								case LabelOutcome.Failure:
									{
										ugsBadgeStates.Add(labelIdx, UgsBadgeState.Failure);
										break;
									}

								case LabelOutcome.Unspecified:
									{
										ugsBadgeStates.Add(labelIdx, UgsBadgeState.Skipped);
										break;
									}
							}
							break;
						}

					case LabelState.Running:
						{
							ugsBadgeStates.Add(labelIdx, UgsBadgeState.Starting);
							break;
						}

					case LabelState.Unspecified:
						{
							ugsBadgeStates.Add(labelIdx, UgsBadgeState.Skipped);
							break;
						}
				}
			}
			return ugsBadgeStates;
		}

		/// <summary>
		/// Gets the state of a job, as a label that includes all steps
		/// </summary>
		/// <param name="job">The job to query</param>
		/// <param name="stepForNode">Map from node to step</param>
		/// <param name="newState">Receives the state of the label</param>
		/// <param name="newOutcome">Receives the outcome of the label</param>
		public static void GetJobState(this IJob job, IReadOnlyDictionary<NodeRef, IJobStep> stepForNode, out LabelState newState, out LabelOutcome newOutcome)
		{
			List<NodeRef> nodes = new List<NodeRef>();
			foreach (IJobStepBatch batch in job.Batches)
			{
				foreach (IJobStep step in batch.Steps)
				{
					nodes.Add(new NodeRef(batch.GroupIdx, step.NodeIdx));
				}
			}
			GetLabelState(nodes, stepForNode, out newState, out newOutcome);
		}

		/// <summary>
		/// Gets the state of a label
		/// </summary>
		/// <param name="includedNodes">Nodes to include in this label</param>
		/// <param name="stepForNode">Map from node to step</param>
		/// <param name="newState">Receives the state of the label</param>
		/// <param name="newOutcome">Receives the outcome of the label</param>
		public static void GetLabelState(IEnumerable<NodeRef> includedNodes, IReadOnlyDictionary<NodeRef, IJobStep> stepForNode, out LabelState newState, out LabelOutcome newOutcome)
		{
			newState = LabelState.Complete;
			newOutcome = LabelOutcome.Success;
			foreach (NodeRef includedNodeRef in includedNodes)
			{
				IJobStep? includedStep;
				if (stepForNode.TryGetValue(includedNodeRef, out includedStep))
				{
					// Update the state
					if (includedStep.State != JobStepState.Completed && includedStep.State != JobStepState.Skipped && includedStep.State != JobStepState.Aborted)
					{
						newState = LabelState.Running;
					}

					// Update the outcome
					if (includedStep.State == JobStepState.Skipped || includedStep.State == JobStepState.Aborted || includedStep.Outcome == JobStepOutcome.Failure)
					{
						newOutcome = LabelOutcome.Failure;
					}
					else if (includedStep.Outcome == JobStepOutcome.Warnings && newOutcome == LabelOutcome.Success)
					{
						newOutcome = LabelOutcome.Warnings;
					}
				}
			}
		}

		/// <summary>
		/// Creates an RPC response object
		/// </summary>
		/// <param name="job">The job document</param>
		/// <returns></returns>
		public static HordeCommon.Rpc.GetJobResponse ToRpcResponse(this IJob job)
		{
			HordeCommon.Rpc.GetJobResponse response = new HordeCommon.Rpc.GetJobResponse();
			response.StreamId = job.StreamId.ToString();
			response.Change = job.Change;
			response.CodeChange = job.CodeChange;
			response.PreflightChange = job.PreflightChange;
			response.ClonedPreflightChange = job.ClonedPreflightChange;
			response.Arguments.Add(job.Arguments);
			return response;
		}

		/// <summary>
		/// Gets a key attached to all artifacts produced for a job
		/// </summary>
		public static string GetArtifactKey(this IJob job)
		{
			return $"job:{job.Id}";
		}

		/// <summary>
		/// Gets a key attached to all artifacts produced for a job step
		/// </summary>
		public static string GetArtifactKey(this IJob job, IJobStep jobStep)
		{
			return $"job:{job.Id}/step:{jobStep.Id}";
		}
	}
}
