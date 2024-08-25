// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
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
using Horde.Server.Streams;
using HordeCommon;
using HordeCommon.Rpc.Tasks;
using MongoDB.Bson;

namespace Horde.Server.Jobs
{
	/// <summary>
	/// Options for creating a new job
	/// </summary>
	public class CreateJobOptions
	{
		/// <inheritdoc cref="IJob.PreflightChange"/>
		public int? PreflightChange { get; set; }

		/// <inheritdoc cref="IJob.ClonedPreflightChange"/>
		public int? ClonedPreflightChange { get; set; }

		/// <inheritdoc cref="IJob.PreflightDescription"/>
		public string? PreflightDescription { get; set; }

		/// <inheritdoc cref="IJob.StartedByUserId"/>
		public UserId? StartedByUserId { get; set; }

		/// <inheritdoc cref="IJob.StartedByBisectTaskId"/>
		public BisectTaskId? StartedByBisectTaskId { get; set; }

		/// <inheritdoc cref="IJob.Priority"/>
		public Priority? Priority { get; set; }

		/// <inheritdoc cref="IJob.AutoSubmit"/>
		public bool? AutoSubmit { get; set; }

		/// <inheritdoc cref="IJob.UpdateIssues"/>
		public bool? UpdateIssues { get; set; }

		/// <inheritdoc cref="IJob.PromoteIssuesByDefault"/>
		public bool? PromoteIssuesByDefault { get; set; }

		/// <inheritdoc cref="IJob.JobOptions"/>
		public JobOptions? JobOptions { get; set; }

		/// <inheritdoc cref="IJob.Claims"/>
		public List<AclClaimConfig> Claims { get; set; } = new List<AclClaimConfig>();

		/// <summary>
		/// List of downstream job triggers
		/// </summary>
		public List<ChainedJobTemplateConfig> JobTriggers { get; } = new List<ChainedJobTemplateConfig>();

		/// <inheritdoc cref="IJob.ShowUgsBadges"/>
		public bool ShowUgsBadges { get; set; }

		/// <inheritdoc cref="IJob.ShowUgsAlerts"/>
		public bool ShowUgsAlerts { get; set; }

		/// <inheritdoc cref="IJob.NotificationChannel"/>
		public string? NotificationChannel { get; set; }

		/// <inheritdoc cref="IJob.NotificationChannelFilter"/>
		public string? NotificationChannelFilter { get; set; }

		/// <inheritdoc cref="IJob.Arguments"/>
		public List<string> Arguments { get; } = new List<string>();

		/// <inheritdoc cref="IJob.Environment"/>
		public Dictionary<string, string> Environment { get; } = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

		/// <summary>
		/// Default constructor
		/// </summary>
		public CreateJobOptions()
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public CreateJobOptions(TemplateRefConfig templateRefConfig)
		{
			JobOptions = templateRefConfig.JobOptions;
			PromoteIssuesByDefault = templateRefConfig.PromoteIssuesByDefault;
			if (templateRefConfig.ChainedJobs != null)
			{
				JobTriggers.AddRange(templateRefConfig.ChainedJobs);
			}
			ShowUgsBadges = templateRefConfig.ShowUgsBadges;
			ShowUgsAlerts = templateRefConfig.ShowUgsAlerts;
			NotificationChannel = templateRefConfig.NotificationChannel;
			NotificationChannelFilter = templateRefConfig.NotificationChannelFilter;
		}
	}

	/// <summary>
	/// Interface for a collection of job documents
	/// </summary>
	public interface IJobCollection
	{
		/// <summary>
		/// Creates a new job
		/// </summary>
		/// <param name="jobId">A requested job id</param>
		/// <param name="streamId">Unique id of the stream that this job belongs to</param>
		/// <param name="templateRefId">Name of the template ref</param>
		/// <param name="templateHash">Template for this job</param>
		/// <param name="graph">The graph for the new job</param>
		/// <param name="name">Name of the job</param>
		/// <param name="change">The change to build</param>
		/// <param name="codeChange">The corresponding code changelist number</param>
		/// <param name="options">Additional options for the new job</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The new job document</returns>
		Task<IJob> AddAsync(JobId jobId, StreamId streamId, TemplateId templateRefId, ContentHash templateHash, IGraph graph, string name, int change, int codeChange, CreateJobOptions options, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets a job with the given unique id
		/// </summary>
		/// <param name="jobId">Job id to search for</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about the given job</returns>
		Task<IJob?> GetAsync(JobId jobId, CancellationToken cancellationToken = default);

		/// <summary>
		/// Deletes a job
		/// </summary>
		/// <param name="job">The job to remove</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task<bool> RemoveAsync(IJob job, CancellationToken cancellationToken = default);

		/// <summary>
		/// Delete all the jobs for a stream
		/// </summary>
		/// <param name="streamId">Unique id of the stream</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Async task</returns>
		Task RemoveStreamAsync(StreamId streamId, CancellationToken cancellationToken = default);

		/// <summary>
		/// Searches for jobs matching the given criteria
		/// </summary>
		/// <param name="jobIds">List of job ids to return</param>
		/// <param name="streamId">The stream containing the job</param>
		/// <param name="name">Name of the job</param>
		/// <param name="templates">Templates to look for</param>
		/// <param name="minChange">The minimum changelist number</param>
		/// <param name="maxChange">The maximum changelist number</param>
		/// <param name="preflightChange">Preflight change to find</param>
		/// <param name="preflightOnly">Whether to only include preflights</param>
		/// <param name="startedByUser">User id for which to include jobs</param>
		/// <param name="preflightStartedByUser">User for which to include preflight jobs</param>
		/// <param name="minCreateTime">The minimum creation time</param>
		/// <param name="maxCreateTime">The maximum creation time</param>
		/// <param name="modifiedBefore">Filter the results by modified time</param>
		/// <param name="modifiedAfter">Filter the results by modified time</param>
		/// <param name="batchState">One or more batches matches this state</param>
		/// <param name="index">Index of the first result to return</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="consistentRead">If the database read should be made to the replica server</param>
		/// <param name="indexHint">Name of index to be specified as a hint to the database query planner</param>
		/// <param name="excludeUserJobs">Whether to exclude user jobs from the find</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of jobs matching the given criteria</returns>
		Task<IReadOnlyList<IJob>> FindAsync(JobId[]? jobIds = null, StreamId? streamId = null, string? name = null, TemplateId[]? templates = null, int? minChange = null, int? maxChange = null, int? preflightChange = null, bool? preflightOnly = null, UserId? preflightStartedByUser = null, UserId? startedByUser = null, DateTimeOffset? minCreateTime = null, DateTimeOffset? maxCreateTime = null, DateTimeOffset? modifiedBefore = null, DateTimeOffset? modifiedAfter = null, JobStepBatchState? batchState = null, int? index = null, int? count = null, bool consistentRead = true, string? indexHint = null, bool? excludeUserJobs = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Searches for jobs matching the given criteria
		/// </summary>
		/// <param name="bisectTaskId">The bisect task to find jobs for</param>
		/// <param name="running">Whether to filter by running jobs</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of jobs matching the given criteria</returns>
		IAsyncEnumerable<IJob> FindBisectTaskJobsAsync(BisectTaskId bisectTaskId, bool? running, CancellationToken cancellationToken = default);

		/// <summary>
		/// Searches for jobs in a specified stream and templates
		/// </summary>
		/// <param name="streamId">The stream containing the job</param>
		/// <param name="templates">Templates to look for</param>
		/// <param name="preflightStartedByUser">User for which to include preflight jobs</param>
		/// <param name="maxCreateTime">The maximum creation time</param>
		/// <param name="modifiedAfter">Filter the results by modified time</param>
		/// <param name="index">Index of the first result to return</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="consistentRead">If the database read should be made to the replica server</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task<IReadOnlyList<IJob>> FindLatestByStreamWithTemplatesAsync(StreamId streamId, TemplateId[] templates, UserId? preflightStartedByUser = null, DateTimeOffset? maxCreateTime = null, DateTimeOffset? modifiedAfter = null, int? index = null, int? count = null, bool consistentRead = false, CancellationToken cancellationToken = default);

		/// <summary>
		/// Updates a new job
		/// </summary>
		/// <param name="job">The job document to update</param>
		/// <param name="graph">Graph for the job</param>
		/// <param name="name">Name of the job</param>
		/// <param name="priority">Priority of the job</param>
		/// <param name="autoSubmit">Automatically submit the job on completion</param>
		/// <param name="autoSubmitChange">Changelist that was automatically submitted</param>
		/// <param name="autoSubmitMessage"></param>
		/// <param name="abortedByUserId">Name of the user that aborted the job</param>
		/// <param name="notificationTriggerId">Id for a notification trigger</param>
		/// <param name="reports">New reports</param>
		/// <param name="arguments">New arguments for the job</param>
		/// <param name="labelIdxToTriggerId">New trigger ID for a label in the job</param>
		/// <param name="jobTrigger">New downstream job id</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task<IJob?> TryUpdateJobAsync(IJob job, IGraph graph, string? name = null, Priority? priority = null, bool? autoSubmit = null, int? autoSubmitChange = null, string? autoSubmitMessage = null, UserId? abortedByUserId = null, ObjectId? notificationTriggerId = null, List<Report>? reports = null, List<string>? arguments = null, KeyValuePair<int, ObjectId>? labelIdxToTriggerId = null, KeyValuePair<TemplateId, JobId>? jobTrigger = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Updates the state of a batch
		/// </summary>
		/// <param name="job">Job to update</param>
		/// <param name="graph">The graph for this job</param>
		/// <param name="batchId">Unique id of the batch to update</param>
		/// <param name="newLogId">The new log file id</param>
		/// <param name="newState">New state of the jobstep</param>
		/// <param name="newError">Error code for the batch</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>True if the job was updated, false if it was deleted</returns>
		Task<IJob?> TryUpdateBatchAsync(IJob job, IGraph graph, JobStepBatchId batchId, LogId? newLogId, JobStepBatchState? newState, JobStepBatchError? newError, CancellationToken cancellationToken = default);

		/// <summary>
		/// Update a jobstep state
		/// </summary>
		/// <param name="job">Job to update</param>
		/// <param name="graph">The graph for this job</param>
		/// <param name="batchId">Unique id of the batch containing the step</param>
		/// <param name="stepId">Unique id of the step to update</param>
		/// <param name="newState">New state of the jobstep</param>
		/// <param name="newOutcome">New outcome of the jobstep</param>
		/// <param name="newError">New error annotation for this jobstep</param>
		/// <param name="newAbortRequested">New state of request abort</param>
		/// <param name="newAbortByUserId">New name of user that requested the abort</param>
		/// <param name="newLogId">New log id for the jobstep</param>
		/// <param name="newNotificationTriggerId">New id for a notification trigger</param>
		/// <param name="newRetryByUserId">Whether the step should be retried</param>
		/// <param name="newPriority">New priority for this step</param>
		/// <param name="newReports">New report documents</param>
		/// <param name="newProperties">Property changes. Any properties with a null value will be removed.</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>True if the job was updated, false if it was deleted in the meantime</returns>
		Task<IJob?> TryUpdateStepAsync(IJob job, IGraph graph, JobStepBatchId batchId, JobStepId stepId, JobStepState newState = default, JobStepOutcome newOutcome = default, JobStepError? newError = null, bool? newAbortRequested = null, UserId? newAbortByUserId = null, LogId? newLogId = null, ObjectId? newNotificationTriggerId = null, UserId? newRetryByUserId = null, Priority? newPriority = null, List<Report>? newReports = null, Dictionary<string, string?>? newProperties = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Attempts to update the node groups to be executed for a job. Fails if another write happens in the meantime.
		/// </summary>
		/// <param name="job">The job to update</param>
		/// <param name="oldGraph">Old graph for this job</param>
		/// <param name="newGraph">New graph for this job</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>True if the groups were updated to the given list. False if another write happened first.</returns>
		Task<IJob?> TryUpdateGraphAsync(IJob job, IGraph oldGraph, IGraph newGraph, CancellationToken cancellationToken = default);

		/// <summary>
		/// Removes a job from the dispatch queue. Ignores the state of any batches still remaining to execute. Should only be used to correct for inconsistent state.
		/// </summary>
		/// <param name="job">Job</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		Task<IJob?> TryRemoveFromDispatchQueueAsync(IJob job, CancellationToken cancellationToken = default);

		/// <summary>
		/// Adds an issue to a job
		/// </summary>
		/// <param name="jobId">The job id</param>
		/// <param name="issueId">The issue to add</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Async task</returns>
		Task AddIssueToJobAsync(JobId jobId, int issueId, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets a queue of jobs to consider for execution
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Sorted list of jobs to execute</returns>
		Task<IReadOnlyList<IJob>> GetDispatchQueueAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Marks a job as skipped
		/// </summary>
		/// <param name="job">The job to update</param>
		/// <param name="graph">Graph for the job</param>
		/// <param name="reason">Reason for this batch being failed</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Updated version of the job</returns>
		Task<IJob?> SkipAllBatchesAsync(IJob? job, IGraph graph, JobStepBatchError reason, CancellationToken cancellationToken = default);

		/// <summary>
		/// Marks a batch as skipped
		/// </summary>
		/// <param name="job">The job to update</param>
		/// <param name="batchId">The batch to mark as skipped</param>
		/// <param name="graph">Graph for the job</param>
		/// <param name="reason">Reason for this batch being failed</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Updated version of the job</returns>
		Task<IJob?> SkipBatchAsync(IJob? job, JobStepBatchId batchId, IGraph graph, JobStepBatchError reason, CancellationToken cancellationToken = default);

		/// <summary>
		/// Abort an agent's lease, and update the payload accordingly
		/// </summary>
		/// <param name="job">The job to update</param>
		/// <param name="batchIdx">Index of the batch to cancel</param>
		/// <param name="graph">Graph for the job</param>
		/// <param name="reason">Reason for this batch being failed</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>True if the job is updated</returns>
		Task<IJob?> TryFailBatchAsync(IJob job, int batchIdx, IGraph graph, JobStepBatchError reason, CancellationToken cancellationToken = default);

		/// <summary>
		/// Attempt to assign a lease to execute a batch
		/// </summary>
		/// <param name="job">The job containing the batch</param>
		/// <param name="batchIdx">Index of the batch</param>
		/// <param name="poolId">The pool id</param>
		/// <param name="agentId">New agent to execute the batch</param>
		/// <param name="sessionId">Session of the agent that is to execute the batch</param>
		/// <param name="leaseId">The lease unique id</param>
		/// <param name="logId">Unique id of the log for the batch</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>True if the batch is updated</returns>
		Task<IJob?> TryAssignLeaseAsync(IJob job, int batchIdx, PoolId poolId, AgentId agentId, SessionId sessionId, LeaseId leaseId, LogId logId, CancellationToken cancellationToken = default);

		/// <summary>
		/// Cancel a lease reservation on a batch (before it has started)
		/// </summary>
		/// <param name="job">The job containing the lease</param>
		/// <param name="batchIdx">Index of the batch to cancel</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>True if the job is updated</returns>
		Task<IJob?> TryCancelLeaseAsync(IJob job, int batchIdx, CancellationToken cancellationToken = default);

		/// <summary>
		/// Upgrade all documents in the collection
		/// </summary>
		/// <returns>Async task</returns>
		Task UpgradeDocumentsAsync();
	}
}
