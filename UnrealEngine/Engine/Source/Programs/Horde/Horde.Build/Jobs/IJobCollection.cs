// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Build.Agents;
using Horde.Build.Agents.Leases;
using Horde.Build.Agents.Pools;
using Horde.Build.Agents.Sessions;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Logs;
using Horde.Build.Streams;
using Horde.Build.Users;
using Horde.Build.Utilities;
using HordeCommon;
using MongoDB.Bson;

namespace Horde.Build.Jobs
{
	using JobId = ObjectId<IJob>;
	using LeaseId = ObjectId<ILease>;
	using LogId = ObjectId<ILogFile>;
	using PoolId = StringId<IPool>;
	using SessionId = ObjectId<ISession>;
	using StreamId = StringId<IStream>;
	using TemplateRefId = StringId<TemplateRef>;
	using UserId = ObjectId<IUser>;

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
		/// <param name="preflightChange">Optional changelist to preflight</param>
		/// <param name="clonedPreflightChange">Optional cloned preflight changelist</param>
		/// <param name="preflightDescription">Description for the change being preflighted</param>
		/// <param name="startedByUserId">User that started the job</param>
		/// <param name="priority">Priority of the job</param>
		/// <param name="autoSubmit">Whether to automatically submit the preflighted change on completion</param>
		/// <param name="updateIssues">Whether to update issues based on the outcome of this job</param>
		/// <param name="promoteIssuesByDefault">Whether to promote issues based on the outcome of this job by default</param>
		/// <param name="jobTriggers">List of downstream job triggers</param>
		/// <param name="showUgsBadges">Whether to show badges in UGS for this job</param>
		/// <param name="showUgsAlerts">Whether to show alerts in UGS for this job</param>
		/// <param name="notificationChannel">Notification channel for this job</param>
		/// <param name="notificationChannelFilter">Notification channel filter for this job</param>
		/// <param name="arguments">Arguments for the job</param>
		/// <returns>The new job document</returns>
		Task<IJob> AddAsync(JobId jobId, StreamId streamId, TemplateRefId templateRefId, ContentHash templateHash, IGraph graph, string name, int change, int codeChange, int? preflightChange, int? clonedPreflightChange, string? preflightDescription, UserId? startedByUserId, Priority? priority, bool? autoSubmit, bool? updateIssues, bool? promoteIssuesByDefault, List<ChainedJobTemplate>? jobTriggers, bool showUgsBadges, bool showUgsAlerts, string? notificationChannel, string? notificationChannelFilter, List<string>? arguments);

		/// <summary>
		/// Gets a job with the given unique id
		/// </summary>
		/// <param name="jobId">Job id to search for</param>
		/// <returns>Information about the given job</returns>
		Task<IJob?> GetAsync(JobId jobId);

		/// <summary>
		/// Deletes a job
		/// </summary>
		/// <param name="job">The job to remove</param>
		Task<bool> RemoveAsync(IJob job);

		/// <summary>
		/// Delete all the jobs for a stream
		/// </summary>
		/// <param name="streamId">Unique id of the stream</param>
		/// <returns>Async task</returns>
		Task RemoveStreamAsync(StreamId streamId);

		/// <summary>
		/// Gets a job's permissions info by ID
		/// </summary>
		/// <param name="jobId">Unique id of the job</param>
		/// <returns>The job document</returns>
		Task<IJobPermissions?> GetPermissionsAsync(JobId jobId);

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
		/// <param name="index">Index of the first result to return</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="consistentRead">If the database read should be made to the replica server</param>
		/// <param name="indexHint">Name of index to be specified as a hint to the database query planner</param>
		/// <param name="excludeUserJobs">Whether to exclude user jobs from the find</param>
		/// <returns>List of jobs matching the given criteria</returns>
		Task<List<IJob>> FindAsync(JobId[]? jobIds = null, StreamId? streamId = null, string? name = null, TemplateRefId[]? templates = null, int? minChange = null, int? maxChange = null, int? preflightChange = null, bool? preflightOnly = null, UserId? preflightStartedByUser = null, UserId? startedByUser = null, DateTimeOffset? minCreateTime = null, DateTimeOffset? maxCreateTime = null, DateTimeOffset? modifiedBefore = null, DateTimeOffset? modifiedAfter = null, int? index = null, int? count = null, bool consistentRead = true, string? indexHint = null, bool? excludeUserJobs = null);

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
		Task<List<IJob>> FindLatestByStreamWithTemplatesAsync(StreamId streamId, TemplateRefId[] templates, UserId? preflightStartedByUser = null, DateTimeOffset? maxCreateTime = null, DateTimeOffset? modifiedAfter = null, int? index = null, int? count = null, bool consistentRead = false);

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
		Task<IJob?> TryUpdateJobAsync(IJob job, IGraph graph, string? name = null, Priority? priority = null, bool? autoSubmit = null, int? autoSubmitChange = null, string? autoSubmitMessage = null, UserId? abortedByUserId = null, ObjectId? notificationTriggerId = null, List<Report>? reports = null, List<string>? arguments = null, KeyValuePair<int, ObjectId>? labelIdxToTriggerId = null, KeyValuePair<TemplateRefId, JobId>? jobTrigger = null);

		/// <summary>
		/// Updates the state of a batch
		/// </summary>
		/// <param name="job">Job to update</param>
		/// <param name="graph">The graph for this job</param>
		/// <param name="batchId">Unique id of the batch to update</param>
		/// <param name="newLogId">The new log file id</param>
		/// <param name="newState">New state of the jobstep</param>
		/// <param name="newError">Error code for the batch</param>
		/// <returns>True if the job was updated, false if it was deleted</returns>
		Task<IJob?> TryUpdateBatchAsync(IJob job, IGraph graph, SubResourceId batchId, LogId? newLogId, JobStepBatchState? newState, JobStepBatchError? newError);

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
		/// <returns>True if the job was updated, false if it was deleted in the meantime</returns>
		Task<IJob?> TryUpdateStepAsync(IJob job, IGraph graph, SubResourceId batchId, SubResourceId stepId, JobStepState newState = default, JobStepOutcome newOutcome = default, JobStepError? newError = null, bool? newAbortRequested = null, UserId? newAbortByUserId = null, LogId? newLogId = null, ObjectId? newNotificationTriggerId = null, UserId? newRetryByUserId = null, Priority? newPriority = null, List<Report>? newReports = null, Dictionary<string, string?>? newProperties = null);

		/// <summary>
		/// Attempts to update the node groups to be executed for a job. Fails if another write happens in the meantime.
		/// </summary>
		/// <param name="job">The job to update</param>
		/// <param name="newGraph">New graph for this job</param>
		/// <returns>True if the groups were updated to the given list. False if another write happened first.</returns>
		Task<IJob?> TryUpdateGraphAsync(IJob job, IGraph newGraph);

		/// <summary>
		/// Removes a job from the dispatch queue. Ignores the state of any batches still remaining to execute. Should only be used to correct for inconsistent state.
		/// </summary>
		/// <param name="job">Job</param>
		/// <returns></returns>
		Task<IJob?> TryRemoveFromDispatchQueueAsync(IJob job);

		/// <summary>
		/// Adds an issue to a job
		/// </summary>
		/// <param name="jobId">The job id</param>
		/// <param name="issueId">The issue to add</param>
		/// <returns>Async task</returns>
		Task AddIssueToJobAsync(JobId jobId, int issueId);

		/// <summary>
		/// Gets a queue of jobs to consider for execution
		/// </summary>
		/// <returns>Sorted list of jobs to execute</returns>
		Task<List<IJob>> GetDispatchQueueAsync();

		/// <summary>
		/// Marks a job as skipped
		/// </summary>
		/// <param name="job">The job to update</param>
		/// <param name="graph">Graph for the job</param>
		/// <param name="reason">Reason for this batch being failed</param>
		/// <returns>Updated version of the job</returns>
		Task<IJob?> SkipAllBatchesAsync(IJob? job, IGraph graph, JobStepBatchError reason);

		/// <summary>
		/// Marks a batch as skipped
		/// </summary>
		/// <param name="job">The job to update</param>
		/// <param name="batchId">The batch to mark as skipped</param>
		/// <param name="graph">Graph for the job</param>
		/// <param name="reason">Reason for this batch being failed</param>
		/// <returns>Updated version of the job</returns>
		Task<IJob?> SkipBatchAsync(IJob? job, SubResourceId batchId, IGraph graph, JobStepBatchError reason);

		/// <summary>
		/// Abort an agent's lease, and update the payload accordingly
		/// </summary>
		/// <param name="job">The job to update</param>
		/// <param name="batchIdx">Index of the batch to cancel</param>
		/// <param name="graph">Graph for the job</param>
		/// <param name="reason">Reason for this batch being failed</param>
		/// <returns>True if the job is updated</returns>
		Task<IJob?> TryFailBatchAsync(IJob job, int batchIdx, IGraph graph, JobStepBatchError reason);

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
		/// <returns>True if the batch is updated</returns>
		Task<IJob?> TryAssignLeaseAsync(IJob job, int batchIdx, PoolId poolId, AgentId agentId, SessionId sessionId, LeaseId leaseId, LogId logId);

		/// <summary>
		/// Cancel a lease reservation on a batch (before it has started)
		/// </summary>
		/// <param name="job">The job containing the lease</param>
		/// <param name="batchIdx">Index of the batch to cancel</param>
		/// <returns>True if the job is updated</returns>
		Task<IJob?> TryCancelLeaseAsync(IJob job, int batchIdx);

		/// <summary>
		/// Upgrade all documents in the collection
		/// </summary>
		/// <returns>Async task</returns>
		Task UpgradeDocumentsAsync();
	}
}
