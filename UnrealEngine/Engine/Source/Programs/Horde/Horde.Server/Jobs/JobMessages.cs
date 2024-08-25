// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Linq;
using System.Text.Json.Serialization;
using EpicGames.Core;
using EpicGames.Horde.Artifacts;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Jobs.Bisect;
using EpicGames.Horde.Jobs.Templates;
using EpicGames.Horde.Streams;
using EpicGames.Horde.Users;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Streams;
using HordeCommon;
using HordeCommon.Rpc.Tasks;

namespace Horde.Server.Jobs
{
	/// <summary>
	/// State of the job
	/// </summary>
	public enum JobState
	{
		/// <summary>
		/// Waiting for resources
		/// </summary>
		Waiting,

		/// <summary>
		/// Currently running one or more steps
		/// </summary>
		Running,

		/// <summary>
		/// All steps have completed
		/// </summary>
		Complete,
	}

	/// <summary>
	/// Parameters required to create a job
	/// </summary>
	public class CreateJobRequest
	{
		/// <summary>
		/// The stream that this job belongs to
		/// </summary>
		[Required]
		public StreamId StreamId { get; set; }

		/// <summary>
		/// The template for this job
		/// </summary>
		[Required]
		public TemplateId TemplateId { get; set; }

		/// <summary>
		/// Name of the job
		/// </summary>
		public string? Name { get; set; }

		/// <summary>
		/// The changelist number to build. Can be null for latest.
		/// </summary>
		public int? Change { get; set; }

		/// <summary>
		/// Parameters to use when selecting the change to execute at.
		/// </summary>
		[Obsolete("Use ChangeQueries instead")]
		public ChangeQueryConfig? ChangeQuery
		{
			get => (ChangeQueries != null && ChangeQueries.Count > 0) ? ChangeQueries[0] : null;
			set => ChangeQueries = (value == null) ? null : new List<ChangeQueryConfig> { value };
		}

		/// <summary>
		/// List of change queries to evaluate
		/// </summary>
		public List<ChangeQueryConfig>? ChangeQueries { get; set; }

		/// <summary>
		/// The preflight changelist number
		/// </summary>
		public int? PreflightChange { get; set; }

		/// <summary>
		/// Job options
		/// </summary>
		public JobOptions? JobOptions { get; set; }

		/// <summary>
		/// Priority for the job
		/// </summary>
		public Priority? Priority { get; set; }

		/// <summary>
		/// Whether to automatically submit the preflighted change on completion
		/// </summary>
		public bool? AutoSubmit { get; set; }

		/// <summary>
		/// Whether to update issues based on the outcome of this job
		/// </summary>
		public bool? UpdateIssues { get; set; }

		/// <summary>
		/// Arguments for the job
		/// </summary>
		public List<string>? Arguments { get; set; }

		/// <summary>
		/// Private constructor for serialization
		/// </summary>
		public CreateJobRequest(StreamId streamId, TemplateId templateId)
		{
			StreamId = streamId;
			TemplateId = templateId;
		}
	}

	/// <summary>
	/// Response from creating a new job
	/// </summary>
	public class CreateJobResponse
	{
		/// <summary>
		/// Unique id for the new job
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="id">Unique id for the new job</param>
		public CreateJobResponse(string id)
		{
			Id = id;
		}
	}

	/// <summary>
	/// Updates an existing job
	/// </summary>
	public class UpdateJobRequest
	{
		/// <summary>
		/// New name for the job
		/// </summary>
		public string? Name { get; set; }

		/// <summary>
		/// New priority for the job
		/// </summary>
		public Priority? Priority { get; set; }

		/// <summary>
		/// Set whether the job should be automatically submitted or not
		/// </summary>
		public bool? AutoSubmit { get; set; }

		/// <summary>
		/// Mark this job as aborted
		/// </summary>
		public bool? Aborted { get; set; }

		/// <summary>
		/// New list of arguments for the job. Only -Target= arguments can be modified after the job has started.
		/// </summary>
		public List<string>? Arguments { get; set; }
	}

	/// <summary>
	/// Information about a report associated with a job
	/// </summary>
	public class GetReportResponse
	{
		/// <summary>
		/// Name of the report
		/// </summary>
		public string Name { get; set; }

		/// <summary>
		/// Report placement
		/// </summary>
		public HordeCommon.Rpc.ReportPlacement Placement { get; set; }

		/// <summary>
		/// The artifact id
		/// </summary>
		public string? ArtifactId { get; set; }

		/// <summary>
		/// Content for the report
		/// </summary>
		public string? Content { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="report"></param>
		public GetReportResponse(IReport report)
		{
			Name = report.Name;
			Placement = report.Placement;
			ArtifactId = report.ArtifactId?.ToString();
			Content = report.Content;
		}
	}

	/// <summary>
	/// Information about a job
	/// </summary>
	public class GetJobResponse
	{
		/// <summary>
		/// Unique Id for the job
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// Unique id of the stream containing this job
		/// </summary>
		public string StreamId { get; set; }

		/// <summary>
		/// Name of the job
		/// </summary>
		public string Name { get; set; }

		/// <summary>
		/// The changelist number to build
		/// </summary>
		public int Change { get; set; }

		/// <summary>
		/// The code changelist
		/// </summary>
		public int? CodeChange { get; set; }

		/// <summary>
		/// The preflight changelist number
		/// </summary>
		public int? PreflightChange { get; set; }

		/// <summary>
		/// The cloned preflight changelist number
		/// </summary>
		public int? ClonedPreflightChange { get; set; }

		/// <summary>
		/// Description of the preflight
		/// </summary>
		public string? PreflightDescription { get; set; }

		/// <summary>
		/// The template type
		/// </summary>
		public string TemplateId { get; set; }

		/// <summary>
		/// Hash of the actual template data
		/// </summary>
		public string TemplateHash { get; set; }

		/// <summary>
		/// Hash of the graph for this job
		/// </summary>
		public string? GraphHash { get; set; }

		/// <summary>
		/// The user that started this job [DEPRECATED]
		/// </summary>
		public string? StartedByUserId { get; set; }

		/// <summary>
		/// The user that started this job [DEPRECATED]
		/// </summary>
		public string? StartedByUser { get; set; }

		/// <summary>
		/// The user that started this job
		/// </summary>
		public GetThinUserInfoResponse? StartedByUserInfo { get; set; }

		/// <summary>
		/// Bisection task id that started this job
		/// </summary>
		public BisectTaskId? StartedByBisectTaskId { get; set; }

		/// <summary>
		/// The user that aborted this job [DEPRECATED]
		/// </summary>
		public string? AbortedByUser { get; set; }

		/// <summary>
		/// The user that aborted this job
		/// </summary>
		public GetThinUserInfoResponse? AbortedByUserInfo { get; set; }

		/// <summary>
		/// Priority of the job
		/// </summary>
		public Priority Priority { get; set; }

		/// <summary>
		/// Whether the change will automatically be submitted or not
		/// </summary>
		public bool AutoSubmit { get; set; }

		/// <summary>
		/// The submitted changelist number
		/// </summary>
		public int? AutoSubmitChange { get; }

		/// <summary>
		/// Message produced by trying to auto-submit the change
		/// </summary>
		public string? AutoSubmitMessage { get; }

		/// <summary>
		/// Time that the job was created
		/// </summary>
		public DateTimeOffset CreateTime { get; set; }

		/// <summary>
		/// The global job state
		/// </summary>
		public JobState State { get; set; }

		/// <summary>
		/// Array of jobstep batches
		/// </summary>
		public List<GetBatchResponse>? Batches { get; set; }

		/// <summary>
		/// List of labels
		/// </summary>
		public List<GetLabelStateResponse>? Labels { get; set; }

		/// <summary>
		/// The default label, containing the state of all steps that are otherwise not matched.
		/// </summary>
		public GetDefaultLabelStateResponse? DefaultLabel { get; set; }

		/// <summary>
		/// List of reports
		/// </summary>
		public List<GetReportResponse>? Reports { get; set; }

		/// <summary>
		/// Artifacts produced by this job
		/// </summary>
		public List<GetJobArtifactResponse>? Artifacts { get; set; }

		/// <summary>
		/// Parameters for the job
		/// </summary>
		public List<string> Arguments { get; set; }

		/// <summary>
		/// The last update time for this job
		/// </summary>
		public DateTimeOffset UpdateTime { get; set; }

		/// <summary>
		/// Whether to use the V2 artifacts endpoint
		/// </summary>
		public bool UseArtifactsV2 { get; set; }

		/// <summary>
		/// Whether issues are being updated by this job
		/// </summary>
		public bool UpdateIssues { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="job">Job to create a response for</param>
		/// <param name="startedByUserInfo">User that started this job</param>
		/// <param name="abortedByUserInfo">User that aborted this job</param>
		public GetJobResponse(IJob job, GetThinUserInfoResponse? startedByUserInfo, GetThinUserInfoResponse? abortedByUserInfo)
		{
			Id = job.Id.ToString();
			StreamId = job.StreamId.ToString();
			Name = job.Name;
			Change = job.Change;
			CodeChange = (job.CodeChange != 0) ? (int?)job.CodeChange : null;
			PreflightChange = (job.PreflightChange != 0) ? (int?)job.PreflightChange : null;
			ClonedPreflightChange = (job.ClonedPreflightChange != 0) ? (int?)job.ClonedPreflightChange : null;
			PreflightDescription = job.PreflightDescription;
			TemplateId = job.TemplateId.ToString();
			TemplateHash = job.TemplateHash?.ToString() ?? String.Empty;
			GraphHash = job.GraphHash.ToString();
			StartedByUserId = job.StartedByUserId?.ToString();
			StartedByUser = startedByUserInfo?.Login;
			StartedByUserInfo = startedByUserInfo;
			StartedByBisectTaskId = job.StartedByBisectTaskId;
			AbortedByUser = abortedByUserInfo?.Login;
			AbortedByUserInfo = abortedByUserInfo;
			CreateTime = new DateTimeOffset(job.CreateTimeUtc);
			State = job.GetState();
			Priority = job.Priority;
			AutoSubmit = job.AutoSubmit;
			AutoSubmitChange = job.AutoSubmitChange;
			AutoSubmitMessage = job.AutoSubmitMessage;
			Reports = job.Reports?.ConvertAll(x => new GetReportResponse(x));
			Arguments = job.Arguments.ToList();
			UpdateTime = new DateTimeOffset(job.UpdateTimeUtc);
			UseArtifactsV2 = job.JobOptions?.UseNewTempStorage ?? true;
			UpdateIssues = job.UpdateIssues;
		}
	}

	/// <summary>
	/// Response describing an artifact produced during a job
	/// </summary>
	/// <param name="Id">Identifier for this artifact, if it has been produced</param>
	/// <param name="Name">Name of the artifact</param>
	/// <param name="Type">Artifact type</param>
	/// <param name="Description">Description to display for the artifact on the dashboard</param>
	/// <param name="Keys">Keys for the artifact</param>
	/// <param name="StepId">Step producing the artifact</param>
	public record class GetJobArtifactResponse
	(
		ArtifactId? Id,
		ArtifactName Name,
		ArtifactType Type,
		string? Description,
		List<string> Keys,
		JobStepId StepId
	);

	/// <summary>
	/// The timing info for a job
	/// </summary>
	public class GetJobTimingResponse
	{
		/// <summary>
		/// The job response
		/// </summary>
		public GetJobResponse? JobResponse { get; set; }

		/// <summary>
		/// Timing info for each step
		/// </summary>
		public Dictionary<string, GetStepTimingInfoResponse> Steps { get; set; }

		/// <summary>
		/// Timing information for each label
		/// </summary>
		public List<GetLabelTimingInfoResponse> Labels { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>		
		/// <param name="jobResponse">The job response</param>
		/// <param name="steps">Timing info for each steps</param>
		/// <param name="labels">Timing info for each label</param>
		public GetJobTimingResponse(GetJobResponse? jobResponse, Dictionary<string, GetStepTimingInfoResponse> steps, List<GetLabelTimingInfoResponse> labels)
		{
			JobResponse = jobResponse;
			Steps = steps;
			Labels = labels;
		}
	}

	/// <summary>
	/// The timing info for 
	/// </summary>
	public class FindJobTimingsResponse
	{
		/// <summary>
		/// Timing info for each job
		/// </summary>
		public Dictionary<string, GetJobTimingResponse> Timings { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="timings">Timing info for each job</param>
		public FindJobTimingsResponse(Dictionary<string, GetJobTimingResponse> timings)
		{
			Timings = timings;
		}
	}

	/// <summary>
	/// Request used to update a jobstep
	/// </summary>
	public class UpdateStepRequest
	{
		/// <summary>
		/// The new jobstep state
		/// </summary>
		public JobStepState State { get; set; } = JobStepState.Unspecified;

		/// <summary>
		/// Outcome from the jobstep
		/// </summary>
		public JobStepOutcome Outcome { get; set; } = JobStepOutcome.Unspecified;

		/// <summary>
		/// If the step has been requested to abort
		/// </summary>
		public bool? AbortRequested { get; set; }

		/// <summary>
		/// Specifies the log file id for this step
		/// </summary>
		public string? LogId { get; set; }

		/// <summary>
		/// Whether the step should be re-run
		/// </summary>
		public bool? Retry { get; set; }

		/// <summary>
		/// New priority for this step
		/// </summary>
		public Priority? Priority { get; set; }

		/// <summary>
		/// Properties to set. Any entries with a null value will be removed.
		/// </summary>
		public Dictionary<string, string?>? Properties { get; set; }
	}

	/// <summary>
	/// Response object when updating a jobstep
	/// </summary>
	public class UpdateStepResponse
	{
		/// <summary>
		/// If a new step is created (due to specifying the retry flag), specifies the batch id
		/// </summary>
		public string? BatchId { get; set; }

		/// <summary>
		/// If a step is retried, includes the new step id
		/// </summary>
		public string? StepId { get; set; }
	}

	/// <summary>
	/// Returns information about a jobstep
	/// </summary>
	public class GetStepResponse
	{
		/// <summary>
		/// The unique id of the step
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// Index of the node which this jobstep is to execute
		/// </summary>
		public int NodeIdx { get; set; }

		/// <summary>
		/// Current state of the job step. This is updated automatically when runs complete.
		/// </summary>
		public JobStepState State { get; set; }

		/// <summary>
		/// Current outcome of the jobstep
		/// </summary>
		public JobStepOutcome Outcome { get; set; }

		/// <summary>
		/// Error describing additional context for why a step failed to complete
		/// </summary>
		public JobStepError Error { get; set; }

		/// <summary>
		/// If the step has been requested to abort
		/// </summary>
		public bool AbortRequested { get; set; }

		/// <summary>
		/// Name of the user that requested the abort of this step [DEPRECATED]
		/// </summary>
		public string? AbortByUser { get; set; }

		/// <summary>
		/// The user that requested this step be run again 
		/// </summary>
		public GetThinUserInfoResponse? AbortedByUserInfo { get; set; }

		/// <summary>
		/// Name of the user that requested this step be run again [DEPRECATED]
		/// </summary>
		public string? RetryByUser { get; set; }

		/// <summary>
		/// The user that requested this step be run again 
		/// </summary>
		public GetThinUserInfoResponse? RetriedByUserInfo { get; set; }

		/// <summary>
		/// The log id for this step
		/// </summary>
		public string? LogId { get; set; }

		/// <summary>
		/// Time at which the batch was ready (UTC).
		/// </summary>
		public DateTimeOffset? ReadyTime { get; set; }

		/// <summary>
		/// Time at which the batch started (UTC).
		/// </summary>
		public DateTimeOffset? StartTime { get; set; }

		/// <summary>
		/// Time at which the batch finished (UTC)
		/// </summary>
		public DateTimeOffset? FinishTime { get; set; }

		/// <summary>
		/// List of reports
		/// </summary>
		public List<GetReportResponse>? Reports { get; set; }

		/// <summary>
		/// User-defined properties for this jobstep.
		/// </summary>
		public Dictionary<string, string>? Properties { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="step">The step to construct from</param>
		/// <param name="abortedByUserInfo">User that aborted this step</param>
		/// <param name="retriedByUserInfo">User that retried this step</param>
		public GetStepResponse(IJobStep step, GetThinUserInfoResponse? abortedByUserInfo, GetThinUserInfoResponse? retriedByUserInfo)
		{
			Id = step.Id.ToString();
			NodeIdx = step.NodeIdx;
			State = step.State;
			Outcome = step.Outcome;
			Error = step.Error;
			AbortRequested = step.AbortRequested;
			AbortByUser = abortedByUserInfo?.Login;
			AbortedByUserInfo = abortedByUserInfo;
			RetryByUser = retriedByUserInfo?.Login;
			RetriedByUserInfo = retriedByUserInfo;
			LogId = step.LogId?.ToString();
			ReadyTime = step.ReadyTimeUtc;
			StartTime = step.StartTimeUtc;
			FinishTime = step.FinishTimeUtc;
			Reports = step.Reports?.ConvertAll(x => new GetReportResponse(x));

			if (step.Properties != null && step.Properties.Count > 0)
			{
				Properties = step.Properties;
			}
		}
	}

	/// <summary>
	/// The state of a particular run
	/// </summary>
	[JsonConverter(typeof(JsonStringEnumConverter))]
	public enum JobStepBatchState
	{
		/// <summary>
		/// Waiting for dependencies of at least one jobstep to complete
		/// </summary>
		Waiting = 0,

		/// <summary>
		/// Ready to execute
		/// </summary>
		Ready = 1,

		/// <summary>
		/// Preparing to execute work
		/// </summary>
		Starting = 2,

		/// <summary>
		/// Executing work
		/// </summary>
		Running = 3,

		/// <summary>
		/// Preparing to stop
		/// </summary>
		Stopping = 4,

		/// <summary>
		/// All steps have finished executing
		/// </summary>
		Complete = 5
	}

#pragma warning disable CA1027
	/// <summary>
	/// Error code for a batch not being executed
	/// </summary>
	[JsonConverter(typeof(JsonStringEnumConverter))]
	public enum JobStepBatchError
	{
		/// <summary>
		/// No error
		/// </summary>
		None = 0,

		/// <summary>
		/// The stream for this job is unknown
		/// </summary>
		UnknownStream = 1,

		/// <summary>
		/// The given agent type for this batch was not valid for this stream
		/// </summary>
		UnknownAgentType = 2,

		/// <summary>
		/// The pool id referenced by the agent type was not found
		/// </summary>
		UnknownPool = 3,

		/// <summary>
		/// There are no agents in the given pool currently online
		/// </summary>
		NoAgentsInPool = 4,

		/// <summary>
		/// There are no agents in this pool that are onlinbe
		/// </summary>
		NoAgentsOnline = 5,

		/// <summary>
		/// Unknown workspace referenced by the agent type
		/// </summary>
		UnknownWorkspace = 6,

		/// <summary>
		/// Cancelled
		/// </summary>
		Cancelled = 7,

		/// <summary>
		/// Lost connection with the agent machine
		/// </summary>
		LostConnection = 8,

		/// <summary>
		/// Lease terminated prematurely but can be retried.
		/// </summary>
		Incomplete = 9,

		/// <summary>
		/// An error ocurred while executing the lease. Cannot be retried.
		/// </summary>
		ExecutionError = 10,

		/// <summary>
		/// The change that the job is running against is invalid
		/// </summary>
		UnknownShelf = 11,

		/// <summary>
		/// Step was no longer needed during a job update
		/// </summary>
		NoLongerNeeded = 12,

		/// <summary>
		/// Syncing the branch failed
		/// </summary>
		SyncingFailed = 13,

		/// <summary>
		/// Legacy alias for <see cref="SyncingFailed"/>
		/// </summary>
		[Obsolete("Use SyncingFailed instead")]
		AgentSetupFailed = SyncingFailed,
	}
#pragma warning restore CA1027

	/// <summary>
	/// Request to update a jobstep batch
	/// </summary>
	public class UpdateBatchRequest
	{
		/// <summary>
		/// The new log file id
		/// </summary>
		public string? LogId { get; set; }

		/// <summary>
		/// The state of this batch
		/// </summary>
		public JobStepBatchState? State { get; set; }
	}

	/// <summary>
	/// Information about a jobstep batch
	/// </summary>
	public class GetBatchResponse
	{
		/// <summary>
		/// Unique id for this batch
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// The unique log file id
		/// </summary>
		public string? LogId { get; set; }

		/// <summary>
		/// Index of the group being executed
		/// </summary>
		public int GroupIdx { get; set; }

		/// <summary>
		/// The state of this batch
		/// </summary>
		public JobStepBatchState State { get; set; }

		/// <summary>
		/// Error code for this batch
		/// </summary>
		public JobStepBatchError Error { get; set; }

		/// <summary>
		/// Steps within this run
		/// </summary>
		public List<GetStepResponse> Steps { get; set; }

		/// <summary>
		/// The agent assigned to execute this group
		/// </summary>
		public string? AgentId { get; set; }

		/// <summary>
		/// Rate for using this agent (per hour)
		/// </summary>
		public double? AgentRate { get; set; }

		/// <summary>
		/// The agent session holding this lease
		/// </summary>
		public string? SessionId { get; set; }

		/// <summary>
		/// The lease that's executing this group
		/// </summary>
		public string? LeaseId { get; set; }

		/// <summary>
		/// The priority of this batch
		/// </summary>
		public int WeightedPriority { get; set; }

		/// <summary>
		/// Time at which the group started (UTC).
		/// </summary>
		public DateTimeOffset? StartTime { get; set; }

		/// <summary>
		/// Time at which the group finished (UTC)
		/// </summary>
		public DateTimeOffset? FinishTime { get; set; }

		/// <summary>
		/// Time at which the group became ready (UTC).
		/// </summary>
		public DateTimeOffset? ReadyTime { get; set; }

		/// <summary>
		/// Converts this batch into a public response object
		/// </summary>
		/// <param name="batch">The batch to construct from</param>
		/// <param name="steps">Steps in this batch</param>
		/// <param name="agentRate">Rate for this agent</param>
		/// <returns>Response instance</returns>
		public GetBatchResponse(IJobStepBatch batch, List<GetStepResponse> steps, double? agentRate)
		{
			Id = batch.Id.ToString();
			LogId = batch.LogId?.ToString();
			GroupIdx = batch.GroupIdx;
			State = batch.State;
			Error = batch.Error;
			Steps = steps;
			AgentId = batch.AgentId?.ToString();
			AgentRate = agentRate;
			SessionId = batch.SessionId?.ToString();
			LeaseId = batch.LeaseId?.ToString();
			WeightedPriority = batch.SchedulePriority;
			StartTime = batch.StartTimeUtc;
			FinishTime = batch.FinishTimeUtc;
			ReadyTime = batch.ReadyTimeUtc;
		}
	}

	/// <summary>
	/// State of an aggregate
	/// </summary>
	public enum LabelState
	{
		/// <summary>
		/// Aggregate is not currently being built (no required nodes are present)
		/// </summary>
		Unspecified,

		/// <summary>
		/// Steps are still running
		/// </summary>
		Running,

		/// <summary>
		/// All steps are complete
		/// </summary>
		Complete
	}

	/// <summary>
	/// Outcome of an aggregate
	/// </summary>
	public enum LabelOutcome
	{
		/// <summary>
		/// Aggregate is not currently being built
		/// </summary>
		Unspecified,

		/// <summary>
		/// A step dependency failed
		/// </summary>
		Failure,

		/// <summary>
		/// A dependency finished with warnings
		/// </summary>
		Warnings,

		/// <summary>
		/// Successful
		/// </summary>
		Success,
	}

	/// <summary>
	/// State of a label within a job
	/// </summary>
	public class GetLabelStateResponse
	{
		/// <summary>
		/// State of the label
		/// </summary>
		public LabelState? State { get; set; }

		/// <summary>
		/// Outcome of the label
		/// </summary>
		public LabelOutcome? Outcome { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="state">State of the label</param>
		/// <param name="outcome">Outcome of the label</param>
		public GetLabelStateResponse(LabelState state, LabelOutcome outcome)
		{
			State = state;
			Outcome = outcome;
		}
	}

	/// <summary>
	/// Information about the default label (ie. with inlined list of nodes)
	/// </summary>
	public class GetDefaultLabelStateResponse : GetLabelStateResponse
	{
		/// <summary>
		/// List of nodes covered by default label
		/// </summary>
		public List<string> Nodes { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="state">State of the label</param>
		/// <param name="outcome">Outcome of the label</param>
		/// <param name="nodes">List of nodes that are covered by the default label</param>
		public GetDefaultLabelStateResponse(LabelState state, LabelOutcome outcome, List<string> nodes)
			: base(state, outcome)
		{
			Nodes = nodes;
		}
	}

	/// <summary>
	/// Information about the timing info for a particular target
	/// </summary>
	public class GetTimingInfoResponse
	{
		/// <summary>
		/// Wait time on the critical path
		/// </summary>
		public float? TotalWaitTime { get; set; }

		/// <summary>
		/// Sync time on the critical path
		/// </summary>
		public float? TotalInitTime { get; set; }

		/// <summary>
		/// Duration to this point
		/// </summary>
		public float? TotalTimeToComplete { get; set; }

		/// <summary>
		/// Average wait time by the time the job reaches this point
		/// </summary>
		public float? AverageTotalWaitTime { get; set; }

		/// <summary>
		/// Average sync time to this point
		/// </summary>
		public float? AverageTotalInitTime { get; set; }

		/// <summary>
		/// Average duration to this point
		/// </summary>
		public float? AverageTotalTimeToComplete { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="timingInfo">Timing info to construct from</param>
		public GetTimingInfoResponse(TimingInfo timingInfo)
		{
			TotalWaitTime = (float?)timingInfo.TotalWaitTime?.TotalSeconds;
			TotalInitTime = (float?)timingInfo.TotalInitTime?.TotalSeconds;
			TotalTimeToComplete = (float?)timingInfo.TotalTimeToComplete?.TotalSeconds;

			AverageTotalWaitTime = (float?)timingInfo.AverageTotalWaitTime?.TotalSeconds;
			AverageTotalInitTime = (float?)timingInfo.AverageTotalInitTime?.TotalSeconds;
			AverageTotalTimeToComplete = (float?)timingInfo.AverageTotalTimeToComplete?.TotalSeconds;
		}
	}

	/// <summary>
	/// Information about the timing info for a particular target
	/// </summary>
	public class GetStepTimingInfoResponse : GetTimingInfoResponse
	{
		/// <summary>
		/// Name of this node
		/// </summary>
		public string? Name { get; set; }

		/// <summary>
		/// Average wait time for this step
		/// </summary>
		public float? AverageStepWaitTime { get; set; }

		/// <summary>
		/// Average init time for this step
		/// </summary>
		public float? AverageStepInitTime { get; set; }

		/// <summary>
		/// Average duration for this step
		/// </summary>
		public float? AverageStepDuration { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="name">Name of the node</param>
		/// <param name="jobTimingInfo">Timing info to construct from</param>
		public GetStepTimingInfoResponse(string? name, TimingInfo jobTimingInfo)
			: base(jobTimingInfo)
		{
			Name = name;
			AverageStepWaitTime = jobTimingInfo.StepTiming?.AverageWaitTime;
			AverageStepInitTime = jobTimingInfo.StepTiming?.AverageInitTime;
			AverageStepDuration = jobTimingInfo.StepTiming?.AverageDuration;
		}
	}

	/// <summary>
	/// Information about the timing info for a label
	/// </summary>
	public class GetLabelTimingInfoResponse : GetTimingInfoResponse
	{
		/// <summary>
		/// Name of the label
		/// </summary>
		[Obsolete("Use DashboardName instead")]
		public string? Name => DashboardName;

		/// <summary>
		/// Category for the label
		/// </summary>
		[Obsolete("Use DashboardCategory instead")]
		public string? Category => DashboardCategory;

		/// <summary>
		/// Name of the label
		/// </summary>
		public string? DashboardName { get; set; }

		/// <summary>
		/// Category for the label
		/// </summary>
		public string? DashboardCategory { get; set; }

		/// <summary>
		/// Name of the label
		/// </summary>
		public string? UgsName { get; set; }

		/// <summary>
		/// Category for the label
		/// </summary>
		public string? UgsProject { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="label">The label to construct from</param>
		/// <param name="timingInfo">Timing info to construct from</param>
		public GetLabelTimingInfoResponse(ILabel label, TimingInfo timingInfo)
			: base(timingInfo)
		{
			DashboardName = label.DashboardName;
			DashboardCategory = label.DashboardCategory;
			UgsName = label.UgsName;
			UgsProject = label.UgsProject;
		}
	}

	/// <summary>
	/// Describes the history of a step
	/// </summary>
	public class GetJobStepRefResponse
	{
		/// <summary>
		/// The job id
		/// </summary>
		public string JobId { get; set; }

		/// <summary>
		/// The batch containing the step
		/// </summary>
		public string BatchId { get; set; }

		/// <summary>
		/// The step identifier
		/// </summary>
		public string StepId { get; set; }

		/// <summary>
		/// The change number being built
		/// </summary>
		public int Change { get; set; }

		/// <summary>
		/// The step log id
		/// </summary>
		public string? LogId { get; set; }

		/// <summary>
		/// The pool id
		/// </summary>
		public string? PoolId { get; set; }

		/// <summary>
		/// The agent id
		/// </summary>
		public string? AgentId { get; set; }

		/// <summary>
		/// Outcome of the step, once complete.
		/// </summary>
		public JobStepOutcome? Outcome { get; set; }

		/// <summary>
		/// The issues which affected this step
		/// </summary>
		public List<int>? IssueIds { get; }

		/// <summary>
		/// Time at which the step started.
		/// </summary>
		public DateTimeOffset StartTime { get; }

		/// <summary>
		/// Time at which the step finished.
		/// </summary>
		public DateTimeOffset? FinishTime { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="jobStepRef">The jobstep ref to construct from</param>
		public GetJobStepRefResponse(IJobStepRef jobStepRef)
		{
			JobId = jobStepRef.Id.JobId.ToString();
			BatchId = jobStepRef.Id.BatchId.ToString();
			StepId = jobStepRef.Id.StepId.ToString();
			Change = jobStepRef.Change;
			LogId = jobStepRef.LogId.ToString();
			PoolId = jobStepRef.PoolId?.ToString();
			AgentId = jobStepRef.AgentId?.ToString();
			Outcome = jobStepRef.Outcome;
			IssueIds = jobStepRef.IssueIds?.Select(id => id).ToList();
			StartTime = jobStepRef.StartTimeUtc;
			FinishTime = jobStepRef.FinishTimeUtc;
		}
	}
}
