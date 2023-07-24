// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Horde.Build.Auditing;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Logs;
using Horde.Build.Streams;
using Horde.Build.Users;
using Horde.Build.Utilities;
using MongoDB.Bson;
using MongoDB.Driver;

namespace Horde.Build.Issues
{
	using JobId = ObjectId<IJob>;
	using LogId = ObjectId<ILogFile>;
	using StreamId = StringId<IStream>;
	using TemplateId = StringId<ITemplateRef>;
	using UserId = ObjectId<IUser>;

	/// <summary>
	/// Fingerprint for an issue
	/// </summary>
	public class NewIssueFingerprint : IIssueFingerprint, IEquatable<IIssueFingerprint>
	{
		/// <inheritdoc/>
		public string Type { get; }

		/// <inheritdoc/>
		public CaseInsensitiveStringSet Keys { get; set; }

		/// <inheritdoc/>
		public CaseInsensitiveStringSet? RejectKeys { get; set; }

		/// <inheritdoc/>
		public CaseInsensitiveStringSet? Metadata { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="type">The type of issue</param>
		/// <param name="keys">Keys which uniquely identify this issue</param>
		/// <param name="rejectKeys">Keys which should not match with this issue</param>
		/// <param name="metadata">Additional metadata added by the issue handler</param>
		public NewIssueFingerprint(string type, IEnumerable<string> keys, IEnumerable<string>? rejectKeys, IEnumerable<string>? metadata)
		{
			Type = type;
			Keys = new CaseInsensitiveStringSet(keys);

			if (rejectKeys != null && rejectKeys.Any())
			{
				RejectKeys = new CaseInsensitiveStringSet(rejectKeys);
			}
			if (metadata != null && metadata.Any())
			{
				Metadata = new CaseInsensitiveStringSet(metadata);
			}
		}

		/// <summary>
		/// Copy constructor
		/// </summary>
		/// <param name="other">The fingerprint to copy from</param>
		public NewIssueFingerprint(IIssueFingerprint other)
			: this(other.Type, other.Keys, other.RejectKeys, other.Metadata)
		{
		}

		/// <summary>
		/// Merges another fingerprint into this one
		/// </summary>
		/// <param name="other">The other fingerprint to merge with</param>
		public void MergeWith(IIssueFingerprint other)
		{
			Keys.UnionWith(other.Keys);
			if (other.RejectKeys != null)
			{
				RejectKeys ??= new CaseInsensitiveStringSet();
				RejectKeys.UnionWith(other.RejectKeys);
			}
			if (other.Metadata != null)
			{
				Metadata ??= new CaseInsensitiveStringSet();
				Metadata.UnionWith(other.Metadata);
			}
		}

		/// <inheritdoc/>
		public override bool Equals(object? other) => Equals(other as IIssueFingerprint);

		/// <inheritdoc/>
		public bool Equals(IIssueFingerprint? otherFingerprint)
		{
			if (!ReferenceEquals(this, otherFingerprint))
			{
				if (otherFingerprint == null || !Type.Equals(otherFingerprint.Type, StringComparison.Ordinal) || !Keys.SetEquals(otherFingerprint.Keys))
				{
					return false;
				}
				if (!ContentsEqual(RejectKeys, otherFingerprint.RejectKeys))
				{
					return false;
				}
				if (!ContentsEqual(Metadata, otherFingerprint.Metadata))
				{
					return false;
				}
			}
			return true;
		}

		/// <summary>
		/// Gets a hashcode for this fingerprint
		/// </summary>
		/// <returns>The hashcode value</returns>
		public override int GetHashCode()
		{
			int result = StringComparer.Ordinal.GetHashCode(Type);
			return HashCode.Combine(result, GetContentsHash(Keys), GetContentsHash(RejectKeys), GetContentsHash(Metadata));
		}

		/// <summary>
		/// Checks if the contents of two sets are equal
		/// </summary>
		/// <param name="setA"></param>
		/// <param name="setB"></param>
		/// <returns></returns>
		static bool ContentsEqual(CaseInsensitiveStringSet? setA, CaseInsensitiveStringSet? setB)
		{
			if (setA == null || setA.Count == 0)
			{
				return setB == null || setB.Count == 0;
			}
			else
			{
				return setB != null && setA.SetEquals(setB);
			}
		}

		/// <summary>
		/// Gets the hash of the contents of a case insensitive set
		/// </summary>
		/// <param name="set"></param>
		/// <returns></returns>
		static int GetContentsHash(CaseInsensitiveStringSet? set)
		{
			int value = 0;
			if (set != null)
			{
				foreach (string element in set)
				{
					value = HashCode.Combine(value, StringComparer.OrdinalIgnoreCase.GetHashCode(element));
				}
			}
			return value;
		}

		/// <summary>
		/// Merges two fingerprints togetherthis issue fingerprint with another
		/// </summary>
		/// <param name="a">The first fingerprint</param>
		/// <param name="b">The second fingerprint to merge with</param>
		/// <returns>Merged fingerprint</returns>
		public static NewIssueFingerprint Merge(IIssueFingerprint a, IIssueFingerprint b)
		{
			NewIssueFingerprint newFingerprint = new NewIssueFingerprint(a);
			newFingerprint.MergeWith(b);
			return newFingerprint;
		}

		/// <inheritdoc/>
		public override string ToString()
		{
			return $"{Type}: \"{String.Join("\", \"", Keys)}\"";
		}
	}

	/// <summary>
	/// Information about a new span
	/// </summary>
	public class NewIssueSpanData
	{
		/// <inheritdoc cref="IIssueSpan.StreamId"/>
		public StreamId StreamId { get; set; }

		/// <inheritdoc cref="IIssueSpan.StreamName"/>
		public string StreamName { get; set; }

		/// <inheritdoc cref="IIssueSpan.TemplateRefId"/>
		public TemplateId TemplateRefId { get; set; }

		/// <inheritdoc cref="IIssueSpan.NodeName"/>
		public string NodeName { get; set; }

		/// <inheritdoc cref="IIssueSpan.Fingerprint"/>
		public NewIssueFingerprint Fingerprint { get; set; }

		/// <inheritdoc cref="IIssueSpan.LastSuccess"/>
		public NewIssueStepData? LastSuccess { get; set; }

		/// <inheritdoc cref="IIssueSpan.FirstFailure"/>
		public NewIssueStepData FirstFailure { get; set; }

		/// <inheritdoc cref="IIssueSpan.NextSuccess"/>
		public NewIssueStepData? NextSuccess { get; set; }

		/// <inheritdoc cref="IIssueSpan.Suspects"/>
		public List<NewIssueSpanSuspectData> Suspects { get; set; } = new List<NewIssueSpanSuspectData>();

		/// <summary>
		/// Constructor
		/// </summary>
		public NewIssueSpanData(StreamId streamId, string streamName, TemplateId templateRefId, string nodeName, NewIssueFingerprint fingerprint, NewIssueStepData firstFailure)
		{
			StreamId = streamId;
			StreamName = streamName;
			TemplateRefId = templateRefId;
			NodeName = nodeName;
			Fingerprint = fingerprint;
			FirstFailure = firstFailure;
		}
	}

	/// <summary>
	/// Identifies a particular changelist and job
	/// </summary>
	public class NewIssueStepData
	{
		/// <inheritdoc cref="IIssueStep.Change"/>
		public int Change { get; set; }

		/// <inheritdoc cref="IIssueStep.Severity"/>
		public IssueSeverity Severity { get; set; }

		/// <inheritdoc cref="IIssueStep.JobName"/>
		public string JobName { get; set; }

		/// <inheritdoc cref="IIssueStep.JobId"/>
		public JobId JobId { get; set; }

		/// <inheritdoc cref="IIssueStep.BatchId"/>
		public SubResourceId BatchId { get; set; }

		/// <inheritdoc cref="IIssueStep.StepId"/>
		public SubResourceId StepId { get; set; }

		/// <inheritdoc cref="IIssueStep.StepTime"/>
		public DateTime StepTime { get; set; }

		/// <inheritdoc cref="IIssueStep.LogId"/>
		public LogId? LogId { get; set; }

		/// <inheritdoc cref="IIssueStep.Annotations"/>
		public NodeAnnotations? Annotations { get; set; }

		/// <inheritdoc cref="IIssueStep.PromoteByDefault"/>
		public bool PromoteByDefault { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="change">The changelist number for this job</param>
		/// <param name="severity">Severity of the issue in this step</param>
		/// <param name="jobName">The job name</param>
		/// <param name="jobId">The unique job id</param>
		/// <param name="batchId">The batch id</param>
		/// <param name="stepId">The step id</param>
		/// <param name="stepTime">Time that the step started</param>
		/// <param name="logId">Unique id of the log file for this step</param>
		/// <param name="annotations">Annotations for this step</param>
		/// <param name="promoted">Whether this step is promoted</param>
		public NewIssueStepData(int change, IssueSeverity severity, string jobName, JobId jobId, SubResourceId batchId, SubResourceId stepId, DateTime stepTime, LogId? logId, IReadOnlyNodeAnnotations? annotations, bool promoted)
		{
			Change = change;
			Severity = severity;
			JobName = jobName;
			JobId = jobId;
			BatchId = batchId;
			StepId = stepId;
			StepTime = stepTime;
			LogId = logId;
			if (annotations != null)
			{
				Annotations = new NodeAnnotations(annotations);
			}
			PromoteByDefault = promoted;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="job">The job being built</param>
		/// <param name="batch">Batch of the job for the step</param>
		/// <param name="step">The step being built</param>
		/// <param name="severity">Severity of the issue in this step</param>
		/// <param name="annotations">Annotations for this step</param>
		/// <param name="promoted">Whether this step is promoted</param>
		public NewIssueStepData(IJob job, IJobStepBatch batch, IJobStep step, IssueSeverity severity, IReadOnlyNodeAnnotations? annotations, bool promoted)
			: this(job.Change, severity, job.Name, job.Id, batch.Id, step.Id, step.StartTimeUtc ?? default, step.LogId, annotations, promoted)
		{
		}

		/// <summary>
		/// Constructor for sentinel steps
		/// </summary>
		/// <param name="jobStepRef">The jobstep reference</param>
		public NewIssueStepData(IJobStepRef jobStepRef)
			: this(jobStepRef.Change, IssueSeverity.Unspecified, jobStepRef.JobName, jobStepRef.Id.JobId, jobStepRef.Id.BatchId, jobStepRef.Id.StepId, jobStepRef.StartTimeUtc, jobStepRef.LogId, null, false)
		{
		}
	}

	/// <summary>
	/// Information about a suspect changelist that may have caused an issue
	/// </summary>
	public class NewIssueSuspectData
	{
		/// <inheritdoc cref="IIssueSuspect.AuthorId"/>
		public UserId AuthorId { get; set; }

		/// <inheritdoc cref="IIssueSuspect.Change"/>
		public int Change { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="authorId">Author of the change</param>
		/// <param name="change">The changelist number</param>
		public NewIssueSuspectData(UserId authorId, int change)
		{
			AuthorId = authorId;
			Change = change;
		}
	}

	/// <summary>
	/// Information about a suspect changelist that may have caused an issue
	/// </summary>
	public class NewIssueSpanSuspectData
	{
		/// <inheritdoc cref="IIssueSpanSuspect.Change"/>
		public int Change { get; set; }

		/// <inheritdoc cref="IIssueSpanSuspect.AuthorId"/>
		public UserId AuthorId { get; set; }

		/// <inheritdoc cref="IIssueSpanSuspect.OriginatingChange"/>
		public int? OriginatingChange { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="change">The changelist number</param>
		/// <param name="authorId">Author of the change</param>
		public NewIssueSpanSuspectData(int change, UserId authorId)
		{
			Change = change;
			AuthorId = authorId;
		}
	}

	/// <summary>
	/// Information about a stream containing an issue
	/// </summary>
	public class NewIssueStream : IIssueStream
	{
		/// <inheritdoc cref="IIssueStream.StreamId"/>
		public StreamId StreamId { get; set; }

		/// <inheritdoc cref="IIssueStream.MergeOrigin"/>
		public bool? MergeOrigin { get; set; }

		/// <inheritdoc cref="IIssueStream.ContainsFix"/>
		public bool? ContainsFix { get; set; }

		/// <inheritdoc cref="IIssueStream.FixFailed"/>
		public bool? FixFailed { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="streamId"></param>
		public NewIssueStream(StreamId streamId)
		{
			StreamId = streamId;
		}
	}

	/// <summary>
	/// Interface for an issue collection
	/// </summary>
	public interface IIssueCollection
	{
		/// <summary>
		/// Attempts to enters a critical section over the issue collection. Used for performing operations that require coordination of several documents (eg. attaching spans to issues).
		/// </summary>
		/// <returns>The disposable critical section</returns>
		Task<IAsyncDisposable> EnterCriticalSectionAsync();

		#region Issues

		/// <summary>
		/// Creates a new issue
		/// </summary>
		/// <param name="summary">Summary text for the issue</param>
		/// <returns>The new issue instance</returns>
		Task<IIssue> AddIssueAsync(string summary);

		/// <summary>
		/// Retrieves and issue by id
		/// </summary>
		/// <param name="issueId">Unique id of the issue</param>
		/// <returns>The issue matching the given id, or null</returns>
		Task<IIssue?> GetIssueAsync(int issueId);

		/// <summary>
		/// Finds the suspects for an issue
		/// </summary>
		/// <param name="issueId">The issue to retrieve suspects for</param>
		/// <returns>List of suspects</returns>
		Task<List<IIssueSuspect>> FindSuspectsAsync(int issueId);

		/// <summary>
		/// Searches for open issues
		/// </summary>
		/// <param name="ids">Set of issue ids to find</param>
		/// <param name="ownerId">The user to find issues for</param>
		/// <param name="streamId">The stream affected by the issue</param>
		/// <param name="minChange">Minimum changelist affected by the issue</param>
		/// <param name="maxChange">Maximum changelist affected by the issue</param>
		/// <param name="resolved">Include issues that are now resolved</param>
		/// <param name="promoted">Include only promoted issues</param>
		/// <param name="index">Index within the results to return</param>
		/// <param name="count">Number of results</param>
		/// <returns>List of streams open in the given stream at the given changelist</returns>
		Task<List<IIssue>> FindIssuesAsync(IEnumerable<int>? ids = null, UserId? ownerId = null, StreamId? streamId = null, int? minChange = null, int? maxChange = null, bool? resolved = null, bool? promoted = null, int? index = null, int? count = null);

		/// <summary>
		/// Searches for open issues
		/// </summary>
		/// <param name="changes">List of suspect changes</param>
		/// <returns>List of issues that are affected by the given changes</returns>
		Task<List<IIssue>> FindIssuesForChangesAsync(List<int> changes);

		/// <summary>
		/// Try to update the state of an issue
		/// </summary>
		/// <param name="issue">The issue to update</param>
		/// <param name="initiatedByUserId">User that is making the change</param>
		/// <param name="newSeverity">New severity for the issue</param>
		/// <param name="newSummary">New summary for the issue</param>
		/// <param name="newUserSummary">New user summary for the issue</param>
		/// <param name="newDescription">New description for the issue</param>
		/// <param name="newPromoted">New promoted state of the issue</param>
		/// <param name="newOwnerId">New owner of the issue</param>
		/// <param name="newNominatedById">Person that nominated the new owner</param>
		/// <param name="newAcknowledged">Whether the issue has been acknowledged</param>
		/// <param name="newDeclinedById">Name of a user that has declined the issue</param>
		/// <param name="newFixChange">Fix changelist for the issue. Pass 0 to clear the fix changelist, -1 for systemic issue.</param>
		/// <param name="newResolvedById">User that resolved the issue (may be ObjectId.Empty to clear)</param>
		/// <param name="newExcludeSpanIds">List of span ids to exclude from this issue</param>
		/// <param name="newLastSeenAt"></param>
		/// <param name="newExternalIssueKey">Issue key for external issue tracking</param>
		/// <param name="newQuarantinedById">The user that quarantined the issue</param>
		/// <param name="newForceClosedById">The user that force closed the issue</param>
		/// <returns>True if the issue was updated</returns>
		Task<IIssue?> TryUpdateIssueAsync(IIssue issue, UserId? initiatedByUserId, IssueSeverity? newSeverity = null, string? newSummary = null, string? newUserSummary = null, string? newDescription = null, bool? newPromoted = null, UserId? newOwnerId = null, UserId? newNominatedById = null, bool? newAcknowledged = null, UserId? newDeclinedById = null, int? newFixChange = null, UserId? newResolvedById = null, List<ObjectId>? newExcludeSpanIds = null, DateTime? newLastSeenAt = null, string? newExternalIssueKey = null, UserId? newQuarantinedById = null, UserId? newForceClosedById = null);

		/// <summary>
		/// Updates derived data for an issue (ie. data computed from the spans attached to it). Also clears the issue's 'modified' state.
		/// </summary>
		/// <param name="issue">Issue to update</param>
		/// <param name="newSummary">New summary for the issue</param>
		/// <param name="newSeverity">New severity for the issue</param>
		/// <param name="newFingerprints">New fingerprints for the issue</param>
		/// <param name="newStreams">New streams for the issue</param>
		/// <param name="newSuspects">New suspects for the issue</param>
		/// <param name="newResolvedAt">Time for the last resolved change</param>
		/// <param name="newVerifiedAt">Time that the issue was resolved</param>
		/// <param name="newLastSeenAt">Last time the issue was seen</param>
		/// <returns>Updated issue, or null if the issue is modified in the interim</returns>
		Task<IIssue?> TryUpdateIssueDerivedDataAsync(IIssue issue, string newSummary, IssueSeverity newSeverity, List<NewIssueFingerprint> newFingerprints, List<NewIssueStream> newStreams, List<NewIssueSuspectData> newSuspects, DateTime? newResolvedAt, DateTime? newVerifiedAt, DateTime newLastSeenAt);

		#endregion

		#region Spans

		/// <summary>
		/// Creates a new span from the given failure
		/// </summary>
		/// <param name="issueId">The issue that the span belongs to</param>
		/// <param name="newSpan">Information about the new span</param>
		/// <returns>New span, or null if the sequence token is not valid</returns>
		Task<IIssueSpan> AddSpanAsync(int issueId, NewIssueSpanData newSpan);

		/// <summary>
		/// Gets a particular span
		/// </summary>
		/// <param name="spanId">Unique id of the span</param>
		/// <returns>New span, or null if the sequence token is not valid</returns>
		Task<IIssueSpan?> GetSpanAsync(ObjectId spanId);

		/// <summary>
		/// Updates the given span. Note that data in the span's issue may be derived from this, and the issue should be updated afterwards.
		/// </summary>
		/// <param name="span">Span to update</param>
		/// <param name="newLastSuccess">New last successful step</param>
		/// <param name="newFailure">New failed step</param>
		/// <param name="newNextSuccess">New next successful step</param>
		/// <param name="newSuspects">New suspects for the span</param>
		/// <param name="newIssueId">The new issue id for this span</param>
		/// <returns>The updated span, or null on failure</returns>
		Task<IIssueSpan?> TryUpdateSpanAsync(IIssueSpan span, NewIssueStepData? newLastSuccess = null, NewIssueStepData? newFailure = null, NewIssueStepData? newNextSuccess = null, List<NewIssueSpanSuspectData>? newSuspects = null, int? newIssueId = null);

		/// <summary>
		/// Gets all the spans for a particular issue
		/// </summary>
		/// <param name="issueId">Issue id</param>
		/// <returns>List of spans</returns>
		Task<List<IIssueSpan>> FindSpansAsync(int issueId);

		/// <summary>
		/// Retrieves multiple spans
		/// </summary>
		/// <param name="spanIds">The span ids</param>
		/// <returns>List of spans</returns>
		Task<List<IIssueSpan>> FindSpansAsync(IEnumerable<ObjectId> spanIds);

		/// <summary>
		/// Finds the open issues for a given stream
		/// </summary>
		/// <param name="streamId">The stream id</param>
		/// <param name="templateId">The template id</param>
		/// <param name="name">Name of the node</param>
		/// <param name="change">Changelist number to query</param>
		/// <returns>List of open issues</returns>
		Task<List<IIssueSpan>> FindOpenSpansAsync(StreamId streamId, TemplateId templateId, string name, int change);

		/// <summary>
		/// Searches for open issues
		/// </summary>
		/// <param name="ids">Set of issue ids to find</param>
		/// <param name="issueIds">The issue ids to retrieve spans for</param>
		/// <param name="streamId">The stream affected by the issue</param>
		/// <param name="minChange">Minimum changelist affected by the issue</param>
		/// <param name="maxChange">Maximum changelist affected by the issue</param>
		/// <param name="resolved">Include issues that are now resolved</param>
		/// <param name="index">Index within the results to return</param>
		/// <param name="count">Number of results</param>
		/// <returns>List of streams open in the given stream at the given changelist</returns>
		Task<List<IIssueSpan>> FindSpansAsync(IEnumerable<ObjectId>? ids = null, IEnumerable<int>? issueIds = null, StreamId? streamId = null, int? minChange = null, int? maxChange = null, bool? resolved = null, int? index = null, int? count = null);

		#endregion

		#region Steps

		/// <summary>
		/// Adds a new step
		/// </summary>
		/// <param name="spanId">Initial span for the step</param>
		/// <param name="newStep">Information about the new step</param>
		/// <returns>New step object</returns>
		Task<IIssueStep> AddStepAsync(ObjectId spanId, NewIssueStepData newStep);

		/// <summary>
		/// Find steps for the given spans
		/// </summary>
		/// <param name="spanIds">Span ids</param>
		/// <returns>List of steps</returns>
		Task<List<IIssueStep>> FindStepsAsync(IEnumerable<ObjectId> spanIds);

		/// <summary>
		/// Find steps for the given spans
		/// </summary>
		/// <param name="jobId">The job id</param>
		/// <param name="batchId">The batch id</param>
		/// <param name="stepId">The step id</param>
		/// <returns>List of steps</returns>
		Task<List<IIssueStep>> FindStepsAsync(JobId jobId, SubResourceId? batchId, SubResourceId? stepId);

		#endregion

		/// <summary>
		/// Gets the logger for a particular issue
		/// </summary>
		/// <param name="issueId">The issue id</param>
		/// <returns>Logger for this issue</returns>
		IAuditLogChannel<int> GetLogger(int issueId);
	}

	/// <summary>
	/// Extension methods for issues
	/// </summary>
	public static class IssueCollectionExtensions
	{
		/// <summary>
		/// Find suspects for the given issue
		/// </summary>
		/// <param name="issueCollection"></param>
		/// <param name="issue"></param>
		/// <returns></returns>
		public static Task<List<IIssueSuspect>> FindSuspectsAsync(this IIssueCollection issueCollection, IIssue issue)
		{
			return issueCollection.FindSuspectsAsync(issue.Id);
		}

		/// <summary>
		/// Find steps for the given spans
		/// </summary>
		/// <param name="issueCollection"></param>
		/// <param name="spanId">Span ids</param>
		/// <returns>List of steps</returns>
		public static Task<List<IIssueStep>> FindStepsAsync(this IIssueCollection issueCollection, ObjectId spanId)
		{
			return issueCollection.FindStepsAsync(new[] { spanId });
		}

		/// <summary>
		/// Searches for open issues affecting a job
		/// </summary>
		/// <param name="issueCollection"></param>
		/// <param name="job"></param>
		/// <param name="graph"></param>
		/// <param name="stepId"></param>
		/// <param name="batchId"></param>
		/// <param name="labelIdx"></param>
		/// <param name="ownerId"></param>
		/// <param name="resolved">Whether to include results that are resolved</param>
		/// <param name="promoted">Whether to filter by promoted issues</param>
		/// <param name="index">Index within the results to return</param>
		/// <param name="count">Number of results</param>
		/// <returns></returns>
		public static async Task<List<IIssue>> FindIssuesForJobAsync(this IIssueCollection issueCollection, IJob job, IGraph graph, SubResourceId? stepId = null, SubResourceId? batchId = null, int? labelIdx = null, UserId? ownerId = null, bool? resolved = null, bool? promoted = null, int? index = null, int? count = null)
		{
			List<IIssueStep> steps = await issueCollection.FindStepsAsync(job.Id, batchId, stepId);
			List<IIssueSpan> spans = await issueCollection.FindSpansAsync(steps.Select(x => x.SpanId));

			if (labelIdx != null)
			{
				HashSet<string> nodeNames = new HashSet<string>(job.GetNodesForLabel(graph, labelIdx.Value).Select(x => graph.GetNode(x).Name));
				spans.RemoveAll(x => !nodeNames.Contains(x.NodeName));
			}

			List<int> issueIds = new List<int>(spans.Select(x => x.IssueId));
			if (issueIds.Count == 0)
			{
				return new List<IIssue>();
			}

			return await issueCollection.FindIssuesAsync(ids: issueIds, ownerId: ownerId, resolved: resolved, promoted: promoted, index: index, count: count);
		}
	}
}
