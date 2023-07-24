// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using System.Linq;
using System.Reflection;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Build.Issues.Handlers;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Logs;
using Horde.Build.Perforce;
using Horde.Build.Server;
using Horde.Build.Streams;
using Horde.Build.Users;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using MongoDB.Bson;
using MongoDB.Bson.Serialization;
using MongoDB.Driver;
using OpenTracing;
using OpenTracing.Util;

namespace Horde.Build.Issues
{
	using StreamId = StringId<IStream>;
	using TemplateId = StringId<ITemplateRef>;
	using UserId = ObjectId<IUser>;
	using WorkflowId = StringId<WorkflowConfig>;

	/// <summary>
	/// Detailed issue information
	/// </summary>
	class IssueDetails : IIssueDetails
	{
		/// <inheritdoc/>
		public IIssue Issue { get; }

		/// <inheritdoc/>
		public IUser? Owner { get; }

		/// <inheritdoc/>
		public IUser? NominatedBy { get; }

		/// <inheritdoc/>
		public IUser? ResolvedBy { get; }

		/// <inheritdoc/>
		public IUser? QuarantinedBy { get; set; }

		/// <inheritdoc/>
		public DateTime? QuarantineTimeUtc { get; set; }

		/// <inheritdoc/>
		public IUser? ForceClosedBy { get; set; }

		/// <inheritdoc/>
		public string? ExternalIssueKey { get; }

		/// <inheritdoc/>
		public IReadOnlyList<IIssueSpan> Spans { get; }

		/// <inheritdoc/>
		public IReadOnlyList<IIssueStep> Steps { get; }

		/// <inheritdoc/>
		public IReadOnlyList<IIssueSuspect> Suspects { get; }

		/// <inheritdoc/>
		public IReadOnlyList<IUser> SuspectUsers { get; }

		/// <inheritdoc/>
		readonly bool _showDesktopAlerts;

		/// <inheritdoc/>
		readonly HashSet<UserId> _notifyUsers;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="issue"></param>
		/// <param name="owner"></param>
		/// <param name="nominatedBy"></param>
		/// <param name="resolvedBy"></param>
		/// <param name="quarantinedBy"></param>
		/// <param name="forceClosedBy"></param>
		/// <param name="spans">Spans for this issue</param>
		/// <param name="steps">Steps for this issue</param>
		/// <param name="suspects"></param>
		/// <param name="suspectUsers"></param>
		/// <param name="showDesktopAlerts"></param>
		/// <param name="externalIssueKey"></param>
		public IssueDetails(IIssue issue, IUser? owner, IUser? nominatedBy, IUser? resolvedBy, IUser? quarantinedBy, IUser? forceClosedBy, IReadOnlyList<IIssueSpan> spans, IReadOnlyList<IIssueStep> steps, IReadOnlyList<IIssueSuspect> suspects, IReadOnlyList<IUser> suspectUsers, bool showDesktopAlerts, string? externalIssueKey)
		{
			Issue = issue;
			Owner = owner;
			NominatedBy = nominatedBy;
			ResolvedBy = resolvedBy;
			QuarantinedBy = quarantinedBy;
			QuarantineTimeUtc = issue.QuarantineTimeUtc;
			ForceClosedBy = forceClosedBy;
			Spans = spans;
			Steps = steps;
			Suspects = suspects;
			SuspectUsers = suspectUsers;
			ExternalIssueKey = externalIssueKey; 
			_showDesktopAlerts = showDesktopAlerts;

			if (issue.OwnerId == null)
			{
				_notifyUsers = new HashSet<UserId>(suspects.Select(x => x.AuthorId));
			}
			else
			{
				_notifyUsers = new HashSet<UserId> { issue.OwnerId.Value };
			}
		}

		/// <summary>
		/// Determines whether the given user should be notified about the given issue
		/// </summary>
		/// <returns>True if the user should be notified for this change</returns>
		public bool ShowNotifications()
		{
			return _showDesktopAlerts && Issue.Fingerprints.Any(x => x.Type != DefaultIssueHandler.TypeConst);
		}

		/// <summary>
		/// Determines if the issue is relevant to the given user
		/// </summary>
		/// <param name="userId">The user to query</param>
		/// <returns>True if the issue is relevant to the given user</returns>
		public bool IncludeForUser(UserId userId)
		{
			return _notifyUsers.Contains(userId);
		}
	}

	/// <summary>
	/// Wraps funtionality for manipulating build health issues
	/// </summary>
	public sealed class IssueService : IHostedService, IDisposable
	{
		/// <summary>
		/// Maximum number of changes to query from Perforce in one go
		/// </summary>
		const int MaxChanges = 250;

		readonly IJobStepRefCollection _jobStepRefs;
		readonly IIssueCollection _issueCollection;
		readonly ICommitService _commitService;
		readonly IStreamCollection _streams;
		readonly IUserCollection _userCollection;
		readonly ILogFileService _logFileService;
		readonly IClock _clock;
		readonly ITicker _ticker;

		/// <summary>
		/// Accessor for the issue collection
		/// </summary>
		public IIssueCollection Collection => _issueCollection;

		/// <summary>
		/// Delegate for issue creation events
		/// </summary>
		public delegate void IssueUpdatedEvent(IIssue issue);

		/// <summary>
		/// 
		/// </summary>
		public event IssueUpdatedEvent? OnIssueUpdated;

		/// <summary>
		/// List of issue handlers
		/// </summary>
		readonly List<IssueHandler> _handlers = new List<IssueHandler>();

		/// <summary>
		/// Available issue serializers
		/// </summary>
		readonly Dictionary<string, IssueHandler> _typeToHandler = new Dictionary<string, IssueHandler>(StringComparer.Ordinal);

		/// <summary>
		/// Cached list of currently open issues
		/// </summary>
		IReadOnlyList<IIssueDetails> _cachedIssues = new List<IIssueDetails>();

		/// <summary>
		/// Cache of templates to show desktop alerts for
		/// </summary>
		Dictionary<StreamId, HashSet<TemplateId>> _cachedDesktopAlerts = new Dictionary<StreamId, HashSet<TemplateId>>();

		/// <summary>
		/// Logger for tracing
		/// </summary>
		readonly ILogger<IssueService> _logger;

		/// <summary>
		/// Accessor for list of cached open issues
		/// </summary>
		public IReadOnlyList<IIssueDetails> CachedOpenIssues => _cachedIssues;

		readonly IOptionsMonitor<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public IssueService(IIssueCollection issueCollection, ICommitService commitService, IJobStepRefCollection jobStepRefs, IStreamCollection streams, IUserCollection userCollection, ILogFileService logFileService, IClock clock, IOptionsMonitor<GlobalConfig> globalConfig, ILogger<IssueService> logger)
		{
			Type[] issueTypes = Assembly.GetExecutingAssembly().GetTypes().Where(x => !x.IsAbstract && typeof(IIssue).IsAssignableFrom(x)).ToArray();
			foreach (Type issueType in issueTypes)
			{
				BsonClassMap.LookupClassMap(issueType);
			}

			// Get all the collections
			_issueCollection = issueCollection;
			_commitService = commitService;
			_jobStepRefs = jobStepRefs;
			_streams = streams;
			_userCollection = userCollection;
			_logFileService = logFileService;
			_clock = clock;
			_ticker = clock.AddTicker<IssueService>(TimeSpan.FromMinutes(1.0), TickAsync, logger);
			_globalConfig = globalConfig;
			_logger = logger;

			// Create all the issue handlers
			Type[] handlerTypes = Assembly.GetExecutingAssembly().GetTypes().Where(x => !x.IsAbstract && typeof(IssueHandler).IsAssignableFrom(x)).ToArray();
			foreach (Type handlerType in handlerTypes)
			{
				IssueHandler matcher = (IssueHandler)Activator.CreateInstance(handlerType)!;
				_handlers.Add(matcher);
			}
			_handlers.SortBy(x => -x.Priority);

			// Build the type name to factory map
			_typeToHandler = _handlers.ToDictionary(x => x.Type, x => x, StringComparer.Ordinal);
		}

		/// <inheritdoc/>
		public Task StartAsync(CancellationToken cancellationToken) => _ticker.StartAsync();

		/// <inheritdoc/>
		public Task StopAsync(CancellationToken cancellationToken) => _ticker.StopAsync();

		/// <inheritdoc/>
		public void Dispose() => _ticker.Dispose();

		/// <summary>
		/// Periodically update the list of cached open issues
		/// </summary>
		/// <param name="stoppingToken">Token to indicate that the service should stop</param>
		/// <returns>Async task</returns>
		async ValueTask TickAsync(CancellationToken stoppingToken)
		{
			DateTime utcNow = _clock.UtcNow;

			Dictionary<StreamId, HashSet<TemplateId>> newCachedDesktopAlerts = new Dictionary<StreamId, HashSet<TemplateId>>();

			GlobalConfig globalConfig = _globalConfig.CurrentValue;

			foreach(StreamConfig streamConfig in globalConfig.Streams)
			{
				HashSet<TemplateId> templates = new HashSet<TemplateId>(streamConfig.Templates.Where(x => x.ShowUgsAlerts).Select(x => x.Id));
				if(templates.Count > 0)
				{
					newCachedDesktopAlerts[streamConfig.Id] = templates;
				}
			}

			_cachedDesktopAlerts = newCachedDesktopAlerts;

			// Resolve any issues that haven't been seen in a week
			List<IIssue> openIssues = await _issueCollection.FindIssuesAsync(resolved: false);
			for (int idx = 0; idx < openIssues.Count; idx++)
			{
				IIssue openIssue = openIssues[idx];

				// Do not automaically close quarantined issues
				if (openIssue.QuarantinedByUserId != null)
				{
					continue;
				}

				if (openIssue.LastSeenAt < utcNow - TimeSpan.FromDays(7.0))
				{
					await _issueCollection.TryUpdateIssueAsync(openIssue, null, newResolvedById: IIssue.ResolvedByTimeoutId);
					openIssues.RemoveAt(idx--);
					continue;
				}
			}

			// Update any issues that are listed as valid for a stream that no longer exists
			HashSet<StreamId> validStreamIds = new HashSet<StreamId>(globalConfig.Streams.Select(x => x.Id));
			for (int idx = 0; idx < openIssues.Count; idx++)
			{
				IIssue? openIssue = openIssues[idx];
				if (openIssue.Streams.Any(x => !validStreamIds.Contains(x.StreamId)))
				{
					openIssue = await UpdateIssueDerivedDataAsync(openIssue);
					if (openIssue != null && openIssue.ResolvedAt == null)
					{
						openIssues[idx] = openIssue;
					}
					else
					{
						openIssues.RemoveAt(idx--);
					}
				}
			}

			// Cache the details for any issues that are still open
			List<IIssueDetails> newCachedOpenIssues = new List<IIssueDetails>();
			foreach(IIssue openIssue in openIssues)
			{
				newCachedOpenIssues.Add(await GetIssueDetailsAsync(openIssue));
			}
			_cachedIssues = newCachedOpenIssues;
		}

		/// <inheritdoc/>
		public async Task<IIssueDetails?> GetCachedIssueDetailsAsync(int issueId)
		{
			IIssueDetails? cachedIssue = _cachedIssues.FirstOrDefault(x => x.Issue.Id == issueId);
			if (cachedIssue == null)
			{
				IIssue? issue = await _issueCollection.GetIssueAsync(issueId);
				if (issue != null)
				{
					cachedIssue = await GetIssueDetailsAsync(issue);
				}
			}
			return cachedIssue;
		}

		/// <inheritdoc/>
		public async Task<IIssueDetails> GetIssueDetailsAsync(IIssue issue)
		{
			IUser? owner = issue.OwnerId.HasValue ? await _userCollection.GetCachedUserAsync(issue.OwnerId.Value) : null;
			IUser? nominatedBy = issue.NominatedById.HasValue ? await _userCollection.GetCachedUserAsync(issue.NominatedById.Value) : null;
			IUser? resolvedBy = (issue.ResolvedById.HasValue && issue.ResolvedById != IIssue.ResolvedByTimeoutId && issue.ResolvedById != IIssue.ResolvedByUnknownId)? await _userCollection.GetCachedUserAsync(issue.ResolvedById.Value) : null;
			IUser? quarantinedBy = issue.QuarantinedByUserId.HasValue ? await _userCollection.GetCachedUserAsync(issue.QuarantinedByUserId.Value) : null;
			IUser? forceClosedBy = issue.ForceClosedByUserId.HasValue ? await _userCollection.GetCachedUserAsync(issue.ForceClosedByUserId.Value) : null;

			List<IIssueSpan> spans = await _issueCollection.FindSpansAsync(issue.Id);
			List<IIssueStep> steps = await _issueCollection.FindStepsAsync(spans.Select(x => x.Id));
			List<IIssueSuspect> suspects = await _issueCollection.FindSuspectsAsync(issue);

			List<IUser> suspectUsers = new List<IUser>();
			foreach (UserId suspectUserId in suspects.Select(x => x.AuthorId).Distinct())
			{
				IUser? suspectUser = await _userCollection.GetCachedUserAsync(suspectUserId);
				if (suspectUser != null)
				{
					suspectUsers.Add(suspectUser);
				}
			}

			return new IssueDetails(issue, owner, nominatedBy, resolvedBy, quarantinedBy, forceClosedBy, spans, steps, suspects, suspectUsers, ShowDesktopAlertsForIssue(issue, spans), issue.ExternalIssueKey);
		}

		/// <inheritdoc/>
		public bool ShowDesktopAlertsForIssue(IIssue issue, IReadOnlyList<IIssueSpan> spans)
		{
			_ = issue;

			bool showDesktopAlerts = false;

			HashSet<(StreamId, TemplateId)> checkedTemplates = new HashSet<(StreamId, TemplateId)>();
			foreach(IIssueSpan span in spans)
			{
				if (span.NextSuccess == null && checkedTemplates.Add((span.StreamId, span.TemplateRefId)))
				{
					HashSet<TemplateId>? templates;
					if (_cachedDesktopAlerts.TryGetValue(span.StreamId, out templates) && templates.Contains(span.TemplateRefId))
					{
						showDesktopAlerts = true;
						break;
					}
				}
			}

			return showDesktopAlerts;
		}

		/// <summary>
		/// Updates the state of an issue
		/// </summary>
		/// <param name="id">The current issue id</param>
		/// <param name="summary">New summary for the issue</param>
		/// <param name="description">New description for the issue</param>
		/// <param name="promoted">Whether the issue should be set as promoted</param>
		/// <param name="ownerId">New owner of the issue</param>
		/// <param name="nominatedById">Person that nominated the new owner</param>
		/// <param name="acknowledged">Whether the issue has been acknowledged</param>
		/// <param name="declinedById">Name of the user that has declined this issue</param>
		/// <param name="fixChange">Fix changelist for the issue</param>
		/// <param name="resolvedById">Whether the issue has been resolved</param>
		/// <param name="addSpanIds">Add spans to this issue</param>
		/// <param name="removeSpanIds">Remove spans from this issue</param>
		/// <param name="externalIssueKey">Key for external issue tracking</param>
		/// <param name="quarantinedById">User who has quarantined the issue</param>
		/// <param name="forceClosedById">User who has force closed the issue</param>
		/// <param name="initiatedById">User initiating the changes, for auditing purposes</param>
		/// <returns>True if the issue was updated</returns>
		public async Task<bool> UpdateIssueAsync(int id, string? summary = null, string? description = null, bool? promoted = null, UserId? ownerId = null, UserId? nominatedById = null, bool? acknowledged = null, UserId? declinedById = null, int? fixChange = null, UserId? resolvedById = null, List<ObjectId>? addSpanIds = null, List<ObjectId>? removeSpanIds = null, string? externalIssueKey = null, UserId? quarantinedById = null, UserId? forceClosedById = null, UserId? initiatedById = null)
		{
			IIssue? issue;
			for (; ; )
			{
				issue = await _issueCollection.GetIssueAsync(id);
				if (issue == null)
				{
					return false;
				}

				issue = await _issueCollection.TryUpdateIssueAsync(issue, initiatedById, newUserSummary: summary, newDescription: description, newPromoted: promoted, newOwnerId: ownerId ?? resolvedById, newNominatedById: nominatedById, newDeclinedById: declinedById, newAcknowledged: acknowledged, newFixChange: fixChange, newResolvedById: resolvedById, newExcludeSpanIds: removeSpanIds, newExternalIssueKey: externalIssueKey, newQuarantinedById: quarantinedById, newForceClosedById: forceClosedById);
				if (issue != null)
				{
					break;
				}
			}

			await using IAsyncDisposable csLock = await _issueCollection.EnterCriticalSectionAsync();

			List<IIssue> updateIssues = new List<IIssue>();
			updateIssues.Add(issue);

			if (forceClosedById != null)
			{
				List<IIssueSpan> spans = await _issueCollection.FindSpansAsync(issue.Id);

				foreach (IIssueSpan span in spans)
				{
					if (span.NextSuccess != null)
					{
						continue;
					}

					IIssueStep? step = span.LastFailure ?? span.FirstFailure;

					if (step == null)
					{
						continue;
					}

					await _issueCollection.TryUpdateSpanAsync(span, newNextSuccess: new NewIssueStepData(step.Change, step.Severity, step.JobName, step.JobId, step.BatchId, step.StepId, step.StepTime, step.LogId, step.Annotations, step.PromoteByDefault));
				}				
			}

			if (addSpanIds != null)
			{
				foreach (ObjectId addSpanId in addSpanIds)
				{
					IIssueSpan? span = await _issueCollection.GetSpanAsync(addSpanId);
					if (span != null)
					{
						IIssue? oldIssue = await _issueCollection.GetIssueAsync(span.IssueId);
						updateIssues.Add(oldIssue ?? issue);
						await _issueCollection.TryUpdateSpanAsync(span, newIssueId: issue.Id);
					}
				}
			}

			if (removeSpanIds != null)
			{
				foreach (ObjectId removeSpanId in removeSpanIds)
				{
					IIssueSpan? span = await _issueCollection.GetSpanAsync(removeSpanId);
					if (span != null)
					{
						IIssue newIssue = await FindOrAddIssueForSpanAsync(span);
						updateIssues.Add(newIssue);
						await _issueCollection.TryUpdateSpanAsync(span, newIssueId: newIssue.Id);
					}
				}
			}

			foreach (IIssue updateIssue in updateIssues.GroupBy(x => x.Id).Select(x => x.First()))
			{
				await UpdateIssueDerivedDataAsync(updateIssue);
			}

			return true;
		}

		/// <summary>
		/// Marks a step as complete
		/// </summary>
		/// <param name="job">The job to update</param>
		/// <param name="graph">Graph for the job</param>
		/// <param name="batchId">Unique id of the batch</param>
		/// <param name="stepId">Unique id of the step</param>
		/// <returns>Async task</returns>
		public async Task UpdateCompleteStep(IJob job, IGraph graph, SubResourceId batchId, SubResourceId stepId)
		{
			using IScope scope = GlobalTracer.Instance.BuildSpan("UpdateCompleteStep").StartActive();
			scope.Span.SetTag("JobId", job.Id.ToString());
			scope.Span.SetTag("BatchId", batchId.ToString());
			scope.Span.SetTag("StepId", stepId.ToString());

			_logger.LogInformation("Updating issues for {JobId}:{BatchId}:{StepId}", job.Id, batchId, stepId);

			StreamConfig? streamConfig;
			if (!_globalConfig.CurrentValue.TryGetStream(job.StreamId, out streamConfig))
			{
				throw new Exception($"Invalid stream id '{job.StreamId}' on job '{job.Id}'");
			}

			// Find the batch for this event
			IJobStepBatch? batch;
			if (!job.TryGetBatch(batchId, out batch))
			{
				throw new ArgumentException($"Invalid batch id {batchId}");
			}

			// Find the step for this event
			IJobStep? step;
			if (!job.TryGetStep(stepId, out step))
			{
				throw new ArgumentException($"Invalid step id {stepId}");
			}

			scope.Span.SetTag("LogId", step.LogId.ToString());

			// Get all the annotations for this template
			TemplateRefConfig? templateRef;
			if (!streamConfig.TryGetTemplate(job.TemplateId, out templateRef))
			{
				return;
			}

			NodeAnnotations annotations = new NodeAnnotations();
			annotations.Merge(templateRef.Annotations);

			// Get the node associated with this step
			INode node = graph.Groups[batch.GroupIdx].Nodes[step.NodeIdx];
			annotations.Merge(graph.Groups[batch.GroupIdx].Nodes[step.NodeIdx].Annotations);

			// Get the workflow for this step
			WorkflowConfig? workflow = null;
			WorkflowId? workflowId = annotations.WorkflowId;
			if (workflowId != null)
			{
				if (!streamConfig.TryGetWorkflow(workflowId.Value, out workflow))
				{
					_logger.LogWarning("Unable to find workflow {WorkflowId} for {JobId}:{BatchId}:{StepId}", workflowId, job.Id, batchId, stepId);
					return;
				}
				annotations.Merge(workflow.Annotations);
			}

			// If the workflow disables issue creation, bail out now
			if (!(annotations.CreateIssues ?? true))
			{
				_logger.LogInformation("Issue creation for step {JobId}:{BatchId}:{StepId} disabled", job.Id, batchId, stepId);
				return;
			}

			// Gets the events for this step grouped by fingerprint
			HashSet<IssueEventGroup> eventGroups = await GetEventGroupsForStepAsync(job, batch, step, node, annotations);

			// Try to update all the events. We may need to restart this due to optimistic transactions, so keep track of any existing spans we do not need to check against.
			await using(IAsyncDisposable issueLock = await _issueCollection.EnterCriticalSectionAsync())
			{
				HashSet<ObjectId> checkedSpanIds = new HashSet<ObjectId>();
				for (; ; )
				{
					// Get the spans that are currently open
					List<IIssueSpan> openSpans = await _issueCollection.FindOpenSpansAsync(job.StreamId, job.TemplateId, node.Name, job.Change);
					_logger.LogDebug("{NumSpans} spans are open in {StreamId} at CL {Change} for template {TemplateId}, node {Node}", openSpans.Count, job.StreamId, job.Change, job.TemplateId, node.Name);

					// Add the events to existing issues, and create new issues for everything else
					if (eventGroups.Count > 0)
					{
						if (!await AddEventsToExistingSpans(job, batch, step, eventGroups, openSpans, checkedSpanIds, annotations, job.PromoteIssuesByDefault))
						{
							continue;
						}
						if (!await AddEventsToNewSpans(streamConfig, job, batch, step, node, openSpans, eventGroups, annotations, job.PromoteIssuesByDefault))
						{
							continue;
						}
					}

					// Try to update the sentinels for any other open steps
					if (!await TryUpdateSentinelsAsync(openSpans, streamConfig, job, batch, step))
					{
						continue;
					}

					break;
				}
			}
		}

		/// <summary>
		/// Gets a set of events for the given step 
		/// </summary>
		/// <param name="job">The job containing the step</param>
		/// <param name="batch">Unique id of the batch</param>
		/// <param name="step">Unique id of the step</param>
		/// <param name="node">The node corresponding to the step</param>
		/// <param name="annotations">Annotations for this node</param>
		/// <returns>Set of new events</returns>
		async Task<HashSet<IssueEventGroup>> GetEventGroupsForStepAsync(IJob job, IJobStepBatch batch, IJobStep step, INode node, IReadOnlyNodeAnnotations annotations)
		{
			// Make sure the step has a log file
			if (step.LogId == null)
			{
				throw new ArgumentException($"Step id {step.Id} does not have any log set");
			}

			// Find the log file for this step
			ILogFile? logFile = await _logFileService.GetLogFileAsync(step.LogId.Value, CancellationToken.None);
			if (logFile == null)
			{
				throw new ArgumentException($"Unable to retrieve log {step.LogId}");
			}

			// Create a list of all the events for this step
			List<IssueEvent> issueEvents = new List<IssueEvent>();

			List<ILogEvent> stepEvents = await _logFileService.FindEventsAsync(logFile);
			foreach (ILogEvent stepEvent in stepEvents)
			{
				ILogEventData stepEventData = await _logFileService.GetEventDataAsync(logFile, stepEvent.LineIndex, stepEvent.LineCount);
				issueEvents.Add(new IssueEvent(stepEvent, stepEventData));
			}

			// Pass all the events to each matcher in turn, and allow it to tag any events it can handle
			List<IssueEvent> remainingEvents = new List<IssueEvent>(issueEvents);
			foreach (IssueHandler handler in _handlers)
			{
				handler.TagEvents(job, node, annotations, remainingEvents);
				remainingEvents.RemoveAll(x => x.Ignored || x.Fingerprint != null);
			}

			// Create all the event groups from the non-ignored events
			Dictionary<NewIssueFingerprint, IssueEventGroup> fingerprintToEventGroup = new Dictionary<NewIssueFingerprint, IssueEventGroup>();
			foreach (IssueEvent issueEvent in issueEvents)
			{
				if (!issueEvent.Ignored && issueEvent.Fingerprint != null)
				{
					IssueEventGroup? eventGroup;
					if (!fingerprintToEventGroup.TryGetValue(issueEvent.Fingerprint, out eventGroup))
					{
						eventGroup = new IssueEventGroup(issueEvent.Fingerprint);
						fingerprintToEventGroup.Add(issueEvent.Fingerprint, eventGroup);
					}
					eventGroup.Events.Add(issueEvent);
				}
			}

			// If the node has an annotation to prevent grouping issues together, update all the fingerprints to match
			string? group = annotations.IssueGroup;
			if (group != null)
			{
				Dictionary<NewIssueFingerprint, IssueEventGroup> newFingerprintToEventGroup = new Dictionary<NewIssueFingerprint, IssueEventGroup>(fingerprintToEventGroup.Count);
				foreach ((NewIssueFingerprint fingerprint, IssueEventGroup eventGroup) in fingerprintToEventGroup)
				{
					IssueEventGroup newEventGroup = new IssueEventGroup(new NewIssueFingerprint($"{fingerprint.Type}:{group}", fingerprint.Keys, fingerprint.RejectKeys, fingerprint.Metadata));
					newEventGroup.Events.AddRange(eventGroup.Events);
					newFingerprintToEventGroup[newEventGroup.Fingerprint] = newEventGroup;
				}
				fingerprintToEventGroup = newFingerprintToEventGroup;
			}

			// Print the list of new events
			HashSet<IssueEventGroup> eventGroups = new HashSet<IssueEventGroup>(fingerprintToEventGroup.Values);

			_logger.LogInformation("UpdateCompleteStep({JobId}, {BatchId}, {StepId}): {NumEvents} events, {NumFingerprints} unique fingerprints", job.Id, batch.Id, step.Id, stepEvents.Count, eventGroups.Count);
			foreach (IssueEventGroup eventGroup in eventGroups)
			{
				_logger.LogInformation("Group {Digest}: Type '{FingerprintType}', keys '{FingerprintKeys}', {NumEvents} events", eventGroup.Digest.ToString(), eventGroup.Fingerprint.Type, String.Join(", ", eventGroup.Fingerprint.Keys), eventGroup.Events.Count);
				foreach (IssueEvent eventItem in eventGroup.Events)
				{
					_logger.LogDebug("Group {Digest}: [{Line}] {Message}", eventGroup.Digest.ToString(), eventItem.Event.LineIndex, eventItem.EventData.Message);
				}
			}

			return eventGroups;
		}

		/// <summary>
		/// Adds events to open spans
		/// </summary>
		/// <param name="job">The job containing the step</param>
		/// <param name="batch">Unique id of the batch</param>
		/// <param name="step">Unique id of the step</param>
		/// <param name="newEventGroups">Set of events to try to add</param>
		/// <param name="openSpans">List of open spans</param>
		/// <param name="checkedSpanIds">Set of span ids that have been checked</param>
		/// <param name="annotations">Annotations for this step</param>
		/// <param name="promoteByDefault"></param>
		/// <returns>True if the adding completed</returns>
		async Task<bool> AddEventsToExistingSpans(IJob job, IJobStepBatch batch, IJobStep step, HashSet<IssueEventGroup> newEventGroups, List<IIssueSpan> openSpans, HashSet<ObjectId> checkedSpanIds, IReadOnlyNodeAnnotations? annotations, bool promoteByDefault)
		{
			for(int spanIdx = 0; spanIdx < openSpans.Count; spanIdx++)
			{
				IIssueSpan openSpan = openSpans[spanIdx];
				if (!checkedSpanIds.Contains(openSpan.Id))
				{
					// Filter out the events which match the span's fingerprint
					List<IssueEventGroup> matchEventGroups = newEventGroups.Where(x => openSpan.Fingerprint.IsMatch(x.Fingerprint)).ToList();
					if (matchEventGroups.Count > 0)
					{
						// Add the new step data
						NewIssueStepData newFailure = new NewIssueStepData(job, batch, step, GetIssueSeverity(matchEventGroups.SelectMany(x => x.Events)), annotations, promoteByDefault);
						await _issueCollection.AddStepAsync(openSpan.Id, newFailure);

						// Update the span if this changes the current range
						IIssueSpan? newSpan = openSpan;
						if (newFailure.Change <= openSpan.FirstFailure.Change || newFailure.Change >= openSpan.LastFailure.Change)
						{
							newSpan = await _issueCollection.TryUpdateSpanAsync(openSpan, newFailure: newFailure);
							if (newSpan == null)
							{
								return false;
							}
							await UpdateIssueDerivedDataAsync(newSpan.IssueId);
						}

						// Write out all the merged events
						foreach (IssueEventGroup eventGroup in matchEventGroups)
						{
							_logger.LogDebug("Matched fingerprint {Digest} ({NumLogEvents} log events) to span {SpanId}", eventGroup.Digest, eventGroup.Events.Count, newSpan.Id);
						}

						// Assign all the events to the span
						await _logFileService.AddSpanToEventsAsync(matchEventGroups.SelectMany(x => x.Events.Select(x => x.Event)), newSpan.Id);

						// Remove the matches from the set of events
						newEventGroups.ExceptWith(matchEventGroups);
					}

					// Add the span id to the list of checked spans, so we don't have to check it again
					checkedSpanIds.Add(openSpan.Id);
				}
			}
			return true;
		}

		/// <summary>
		/// Gets an issue severity from its events
		/// </summary>
		/// <param name="events">Set of event severity values</param>
		/// <returns>Severity of this issue</returns>
		static IssueSeverity GetIssueSeverity(IEnumerable<IssueEvent> events)
		{
			IssueSeverity severity;
			if (events.Any(x => x.Severity == EventSeverity.Error))
			{
				severity = IssueSeverity.Error;
			}
			else if (events.Any(x => x.Severity == EventSeverity.Warning))
			{
				severity = IssueSeverity.Warning;
			}
			else
			{
				severity = IssueSeverity.Unspecified;
			}
			return severity;
		}

		/// <summary>
		/// Adds new spans for the given events
		/// </summary>
		/// <param name="streamConfig">The stream containing the job</param>
		/// <param name="job">The job instance</param>
		/// <param name="batch">The job batch</param>
		/// <param name="step">The job step</param>
		/// <param name="node">Node run in the step</param>
		/// <param name="openSpans">List of open spans. New issues will be added to this list.</param>
		/// <param name="newEventGroups">Set of remaining events</param>
		/// <param name="annotations"></param>
		/// <param name="promoteByDefault"></param>
		/// <returns>True if all events were added</returns>
		async Task<bool> AddEventsToNewSpans(StreamConfig streamConfig, IJob job, IJobStepBatch batch, IJobStep step, INode node, List<IIssueSpan> openSpans, HashSet<IssueEventGroup> newEventGroups, IReadOnlyNodeAnnotations? annotations, bool promoteByDefault)
		{
			while (newEventGroups.Count > 0)
			{
				// Keep track of the event groups we merge together
				List<IssueEventGroup> sourceEventGroups = new List<IssueEventGroup>();
				sourceEventGroups.Add(newEventGroups.First());

				// Take the first event, and find all other events that match against it
				IssueEventGroup eventGroup = sourceEventGroups[0];
				foreach (IssueEventGroup otherEventGroup in newEventGroups.Skip(1))
				{
					if (otherEventGroup.Fingerprint.IsMatchForNewSpan(eventGroup.Fingerprint))
					{
						IssueEventGroup newEventGroup = eventGroup.MergeWith(otherEventGroup);
						_logger.LogDebug("Merging group {Group} with group {OtherGroup} to form {NewGroup}", eventGroup.Digest.ToString(), otherEventGroup.Digest.ToString(), newEventGroup.Digest.ToString());
						sourceEventGroups.Add(otherEventGroup);
						eventGroup = newEventGroup;
					}
				}

				// Get the step data
				NewIssueStepData stepData = new NewIssueStepData(job, batch, step, GetIssueSeverity(eventGroup.Events), annotations, promoteByDefault);

				// Get the span data
				NewIssueSpanData spanData = new NewIssueSpanData(streamConfig.Id, streamConfig.Name, job.TemplateId, node.Name, eventGroup.Fingerprint, stepData);

				IJobStepRef? prevJob = await _jobStepRefs.GetPrevStepForNodeAsync(job.StreamId, job.TemplateId, node.Name, job.Change, JobStepOutcome.Success, true);
				if (prevJob != null)
				{
					spanData.LastSuccess = new NewIssueStepData(prevJob);
					spanData.Suspects = await FindSuspectsForSpanAsync(streamConfig, spanData.Fingerprint, spanData.LastSuccess.Change + 1, spanData.FirstFailure.Change);
				}

				IJobStepRef? nextJob = await _jobStepRefs.GetNextStepForNodeAsync(job.StreamId, job.TemplateId, node.Name, job.Change, JobStepOutcome.Success, true);
				if (nextJob != null)
				{
					spanData.NextSuccess = new NewIssueStepData(nextJob);
				}

				// Add all the new objects
				IIssue newIssue = await FindOrAddIssueForSpanAsync(spanData);
				IIssueSpan newSpan = await _issueCollection.AddSpanAsync(newIssue.Id, spanData);
				IIssueStep newStep = await _issueCollection.AddStepAsync(newSpan.Id, stepData);
				await UpdateIssueDerivedDataAsync(newIssue);

				// Update the log events
				_logger.LogDebug("Created new span {SpanId} from event group {Group}", newSpan.Id, eventGroup.Digest.ToString());
				await _logFileService.AddSpanToEventsAsync(eventGroup.Events.Select(x => x.Event), newSpan.Id);

				// Remove the events from the remaining list of events to match
				newEventGroups.ExceptWith(sourceEventGroups);
				openSpans.Add(newSpan);
			}
			return true;
		}

		async Task<IIssue?> UpdateIssueDerivedDataAsync(int issueId)
		{
			IIssue? issue = await _issueCollection.GetIssueAsync(issueId);
			if(issue != null)
			{
				issue = await UpdateIssueDerivedDataAsync(issue);
			}
			return issue;
		}

		static void GetChangeMap(IIssueSpan span, Dictionary<int, int> originChangeToSuspectChange)
		{
			foreach (IIssueSpanSuspect suspect in span.Suspects)
			{
				originChangeToSuspectChange[suspect.OriginatingChange ?? suspect.Change] = suspect.Change;
			}
		}

		static int CompareSpans(Dictionary<int, int> originChangeToSuspectChange, IIssueSpan span)
		{
			foreach (IIssueSpanSuspect suspect in span.Suspects)
			{
				int originChange = suspect.OriginatingChange ?? suspect.Change;
				if (originChangeToSuspectChange.TryGetValue(originChange, out int suspectChange) && suspectChange != suspect.Change)
				{
					return suspectChange - suspect.Change;
				}
			}
			return 0;
		}

		internal static List<IIssueSpan> FindMergeOriginSpans(List<IIssueSpan> spans)
		{
			// Determine the stream(s) highest up in the merge hierarchy, as determined by the minimum changelist number
			// for any robomerged change.
			List<IIssueSpan> originSpans = new List<IIssueSpan>();
			if (spans.Count > 0)
			{
				originSpans.Add(spans[0]);

				Dictionary<int, int> originChangeToSuspectChange = new Dictionary<int, int>();
				GetChangeMap(spans[0], originChangeToSuspectChange);

				for (int idx = 1; idx < spans.Count; idx++)
				{
					IIssueSpan span = spans[idx];
					int comparison = CompareSpans(originChangeToSuspectChange, span);
					if (comparison > 0)
					{
						originChangeToSuspectChange.Clear();
						originSpans.Clear();
					}
					if (comparison >= 0)
					{
						GetChangeMap(span, originChangeToSuspectChange);
						originSpans.Add(span);
					}
				}
			}
			return originSpans;
		}

		async Task<IIssue?> UpdateIssueDerivedDataAsync(IIssue issue)
		{
			Dictionary<(StreamId, int), bool> fixChangeCache = new Dictionary<(StreamId, int), bool>();
			for (; ; )
			{
				GlobalConfig globalConfig = _globalConfig.CurrentValue;

				// Find all the spans that are attached to the issue
				List<IIssueSpan> spans = await _issueCollection.FindSpansAsync(issue.Id);

				// Remove any spans for streams that have been deleted
				for (int idx = spans.Count - 1; idx >= 0; idx--)
				{
					if (!globalConfig.TryGetStream(spans[idx].StreamId, out _))
					{
						spans.RemoveAt(idx);
					}
				}

				// Update the suspects for this issue
				List<NewIssueSuspectData> newSuspects = new List<NewIssueSuspectData>();
				if (spans.Count > 0)
				{
					HashSet<int> suspectChanges = new HashSet<int>(spans[0].Suspects.Select(x => x.OriginatingChange ?? x.Change));
					for (int spanIdx = 1; spanIdx < spans.Count; spanIdx++)
					{
						suspectChanges.IntersectWith(spans[spanIdx].Suspects.Select(x => x.OriginatingChange ?? x.Change));
					}
					newSuspects = spans[0].Suspects.Where(x => suspectChanges.Contains(x.OriginatingChange ?? x.Change)).Select(x => new NewIssueSuspectData(x.AuthorId, x.OriginatingChange ?? x.Change)).ToList();
				}

				// Create the combined fingerprint for this issue
				List<NewIssueFingerprint> newFingerprints = new List<NewIssueFingerprint>();
				foreach (IIssueSpan span in spans)
				{
					NewIssueFingerprint? fingerprint = newFingerprints.FirstOrDefault(x => x.Type == span.Fingerprint.Type);
					if (fingerprint == null)
					{
						newFingerprints.Add(new NewIssueFingerprint(span.Fingerprint));
					}
					else
					{
						fingerprint.MergeWith(span.Fingerprint);
					}
				}

				// Find the severity for this issue
				IssueSeverity newSeverity;
				if (spans.Count == 0 || spans.All(x => x.NextSuccess != null))
				{
					newSeverity = issue.Severity;
				}
				else
				{
					newSeverity = spans.Any(x => x.NextSuccess == null && x.LastFailure.Severity == IssueSeverity.Error) ? IssueSeverity.Error : IssueSeverity.Warning;
				}

				// Find the default summary for this issue
				string newSummary;
				if (newFingerprints.Count == 0)
				{
					newSummary = issue.Summary;
				}
				else if (newFingerprints.Count == 1)
				{
					newSummary = GetSummary(newFingerprints[0], newSeverity);
				}
				else
				{
					newSummary = (newSeverity == IssueSeverity.Error) ? "Errors in multiple steps" : "Warnings in multiple steps";
				}

				// Calculate the last time that this issue was seen
				DateTime newLastSeenAt = issue.CreatedAt;
				foreach (IIssueSpan span in spans)
				{
					if (span.LastFailure.StepTime > newLastSeenAt)
					{
						newLastSeenAt = span.LastFailure.StepTime;
					}
				}

				// Calculate the time at which the issue is verified fixed
				DateTime? newVerifiedAt = null;
				if (issue.QuarantinedByUserId == null)
				{
					if (spans.Count == 0)
					{
						newVerifiedAt = issue.VerifiedAt ?? _clock.UtcNow;
					}
					else
					{
						foreach (IIssueSpan span in spans)
						{
							if (span.NextSuccess == null)
							{
								newVerifiedAt = null;
								break;
							}
							else if (newVerifiedAt == null || span.NextSuccess.StepTime < newVerifiedAt.Value)
							{
								newVerifiedAt = span.NextSuccess.StepTime;
							}
						}
					}
				}

				// Get the new resolved timestamp
				DateTime? newResolvedAt = newVerifiedAt;
				if (issue.ResolvedById != null && issue.ResolvedAt != null)
				{
					newResolvedAt = issue.ResolvedAt;
				}

				// Get the new list of streams
				List<NewIssueStream> newStreams = spans.Select(x => x.StreamId).OrderBy(x => x.ToString(), StringComparer.Ordinal).Distinct().Select(x => new NewIssueStream(x)).ToList();
				if (issue.FixChange != null)
				{
					foreach (NewIssueStream newStream in newStreams)
					{
						IIssueStream? stream = issue.Streams.FirstOrDefault(x => x.StreamId == newStream.StreamId);
						if (stream != null)
						{
							newStream.ContainsFix = stream.ContainsFix;
						}

						newStream.ContainsFix ??= await ContainsFixChange(globalConfig, newStream.StreamId, issue.FixChange.Value, fixChangeCache);

						if (spans.Any(x => x.StreamId == newStream.StreamId && x.LastFailure.Change > issue.FixChange.Value))
						{
							newStream.FixFailed = true;
						}
					}
				}

				// Find the stream where the error was introduced.
				List<IIssueSpan> mergeOrigins = FindMergeOriginSpans(spans);
				foreach (IIssueSpan mergeOrigin in mergeOrigins)
				{
					NewIssueStream? newStream = newStreams.FirstOrDefault(x => x.StreamId == mergeOrigin.StreamId);
					if (newStream != null)
					{
						newStream.MergeOrigin = true;
					}
				}

				// If a fix changelist has been specified and it's not valid, clear it out
				if (newVerifiedAt == null && newResolvedAt != null)
				{
					if (spans.Any(x => HasFixFailed(x, issue.FixChange, newResolvedAt.Value, newStreams)))
					{
						newStreams.ForEach(x => x.ContainsFix = null);
						newResolvedAt = null;
					}
				}

				// Update the issue
				IIssue? newIssue = await _issueCollection.TryUpdateIssueDerivedDataAsync(issue, newSummary, newSeverity, newFingerprints, newStreams, newSuspects, newResolvedAt, newVerifiedAt, newLastSeenAt);
				if (newIssue != null)
				{
					OnIssueUpdated?.Invoke(newIssue);
					return newIssue;
				}

				// Fetch the issue and try again
				newIssue = await _issueCollection.GetIssueAsync(issue.Id);
				if (newIssue == null)
				{
					return null;
				}
				issue = newIssue;
			}
		}

		/// <summary>
		/// Determine if an issue should be marked as fix failed
		/// </summary>
		static bool HasFixFailed(IIssueSpan span, int? fixChange, DateTime resolvedAt, IReadOnlyList<NewIssueStream> streams)
		{
			if (span.NextSuccess == null)
			{
				NewIssueStream? stream = streams.FirstOrDefault(x => x.StreamId == span.StreamId);
				if (stream != null && fixChange != null && (stream.ContainsFix ?? false))
				{
					if (span.LastFailure.Change > fixChange.Value)
					{
						return true;
					}
				}
				else
				{
					if (span.LastFailure.StepTime > resolvedAt + TimeSpan.FromHours(24.0))
					{
						return true;
					}
				}
			}
			return false;
		}

		/// <summary>
		/// Figure out if a stream contains a fix changelist
		/// </summary>
		async ValueTask<bool> ContainsFixChange(GlobalConfig globalConfig, StreamId streamId, int fixChange, Dictionary<(StreamId, int), bool> cachedContainsFixChange)
		{
			bool containsFixChange;
			if (!cachedContainsFixChange.TryGetValue((streamId, fixChange), out containsFixChange) && fixChange > 0)
			{
				StreamConfig? streamConfig;
				if (globalConfig.TryGetStream(streamId, out streamConfig))
				{
					_logger.LogInformation("Querying fix changelist {FixChange} in {StreamId}", fixChange, streamId);
					ICommit? change = await _commitService.GetCollection(streamConfig).FindAsync(fixChange, fixChange, 1).FirstOrDefaultAsync();
					containsFixChange = change != null;
				}
				cachedContainsFixChange[(streamId, fixChange)] = containsFixChange;
			}
			return containsFixChange;
		}

		/// <summary>
		/// Find suspects that can match a given span
		/// </summary>
		/// <param name="streamConfig">The stream name</param>
		/// <param name="fingerprint">The fingerprint for the span</param>
		/// <param name="minChange">Minimum changelist to consider</param>
		/// <param name="maxChange">Maximum changelist to consider</param>
		/// <returns>List of suspects</returns>
		async Task<List<NewIssueSpanSuspectData>> FindSuspectsForSpanAsync(StreamConfig streamConfig, IIssueFingerprint fingerprint, int minChange, int maxChange)
		{
			List<NewIssueSpanSuspectData> suspects = new List<NewIssueSpanSuspectData>();
			if (TryGetHandler(fingerprint, out IssueHandler? handler))
			{
				_logger.LogDebug("Querying for changes in {StreamName} between {MinChange} and {MaxChange}", streamConfig.Name, minChange, maxChange);

				// Get the submitted changes before this job
				List<ICommit> changes = await _commitService.GetCollection(streamConfig).FindAsync(minChange, maxChange, MaxChanges).ToListAsync();
				_logger.LogDebug("Found {NumResults} changes", changes.Count);

				// Get the handler to rank them
				List<SuspectChange> suspectChanges = new List<SuspectChange>(changes.Count);
				foreach (ICommit commit in changes)
				{
					IReadOnlyList<string> files = await commit.GetFilesAsync(CancellationToken.None);
					suspectChanges.Add(new SuspectChange(commit, files));
				}
				handler.RankSuspects(fingerprint, suspectChanges);

				// Output the rankings
				if (suspectChanges.Count > 0)
				{
					int maxRank = suspectChanges.Max(x => x.Rank);
					foreach (SuspectChange suspectChange in suspectChanges)
					{
						_logger.LogDebug("Suspect CL: {Change}, Author: {UserId}, Rank: {Rank}, MaxRank: {MaxRank}", suspectChange.Details.Number, suspectChange.Details.AuthorId, suspectChange.Rank, maxRank);
						if (suspectChange.Rank == maxRank)
						{
							NewIssueSpanSuspectData suspect = new NewIssueSpanSuspectData(suspectChange.Details.Number, suspectChange.Details.OwnerId);
							suspect.OriginatingChange = suspectChange.Details.OriginalChange;
							suspects.Add(suspect);
						}
					}
				}
			}
			return suspects;
		}

		/// <summary>
		/// Find an existing issue that may match one of the given suspect changes
		/// </summary>
		/// <param name="span">The span to find an issue for</param>
		/// <returns>The matching issue</returns>
		async Task<IIssue> FindOrAddIssueForSpanAsync(IIssueSpan span)
		{
			List<IIssue> existingIssues;
			if (span.LastSuccess == null)
			{
				existingIssues = await _issueCollection.FindIssuesAsync(streamId: span.StreamId, minChange: span.FirstFailure.Change, maxChange: span.FirstFailure.Change, resolved: false);
			}
			else
			{
				existingIssues = await _issueCollection.FindIssuesForChangesAsync(span.Suspects.ConvertAll(x => x.OriginatingChange ?? x.Change));
			}

			IIssue? issue = existingIssues.FirstOrDefault(x => x.Fingerprints.Any(y => y.IsMatch(span.Fingerprint)));
			if (issue == null)
			{
				string summary = GetSummary(span.Fingerprint, span.FirstFailure.Severity);
				issue = await _issueCollection.AddIssueAsync(summary);
			}
			return issue;
		}

		/// <summary>
		/// Find an existing issue that may match one of the given suspect changes
		/// </summary>
		/// <param name="span">The span to find an issue for</param>
		/// <returns>The matching issue</returns>
		async Task<IIssue> FindOrAddIssueForSpanAsync(NewIssueSpanData span)
		{
			List<IIssue> existingIssues;
			if (span.LastSuccess == null)
			{
				existingIssues = await _issueCollection.FindIssuesAsync(streamId: span.StreamId, minChange: span.FirstFailure.Change, maxChange: span.FirstFailure.Change, resolved: false);
				_logger.LogDebug("Found {NumIssues} open issues at {Change} in {StreamId}", existingIssues.Count, span.FirstFailure.Change, span.StreamId);
			}
			else
			{
				existingIssues = await _issueCollection.FindIssuesForChangesAsync(span.Suspects.ConvertAll(x => x.OriginatingChange ?? x.Change));
				_logger.LogDebug("Found {NumIssues} open issues in {StreamId} from [{ChangeList}]", existingIssues.Count, span.StreamId, String.Join(", ", span.Suspects.ConvertAll(x => (x.OriginatingChange ?? x.Change).ToString())));
			}
			
			IIssue? issue = existingIssues.FirstOrDefault(x => x.VerifiedAt == null && x.Fingerprints.Any(y => y.IsMatch(span.Fingerprint)));
			if (issue == null)
			{
				string summary = GetSummary(span.Fingerprint, span.FirstFailure.Severity);
				issue = await _issueCollection.AddIssueAsync(summary);
				_logger.LogInformation("Created issue {IssueId}", issue.Id);
			}
			else
			{
				_logger.LogInformation("Natched to issue {IssueId}", issue.Id);
			}
			return issue;
		}

		/// <summary>
		/// Gets the summary text for a particular fingerprint
		/// </summary>
		/// <param name="fingerprint">The issue fingerprint</param>
		/// <param name="severity">Severity of the issue</param>
		/// <returns>The summary text</returns>
		string GetSummary(IIssueFingerprint fingerprint, IssueSeverity severity)
		{
			if (TryGetHandler(fingerprint, out IssueHandler? handler))
			{
				return handler.GetSummary(fingerprint, severity);
			}
			else
			{
				return $"Unknown issue type '{fingerprint.Type}";
			}
		}

		/// <summary>
		/// Update the sentinels for the given list of issues
		/// </summary>
		/// <param name="spans"></param>
		/// <param name="streamConfig">The stream instance</param>
		/// <param name="job"></param>
		/// <param name="batch"></param>
		/// <param name="step"></param>
		/// <returns></returns>
		async Task<bool> TryUpdateSentinelsAsync(List<IIssueSpan> spans, StreamConfig streamConfig, IJob job, IJobStepBatch batch, IJobStep step)
		{
			foreach(IIssueSpan span in spans)
			{
				if (job.Change < span.FirstFailure.Change && (span.LastSuccess == null || job.Change > span.LastSuccess.Change))
				{
					NewIssueStepData newLastSuccess = new NewIssueStepData(job.Change, IssueSeverity.Unspecified, job.Name, job.Id, batch.Id, step.Id, step.StartTimeUtc ?? default, step.LogId, null, false);
					List<NewIssueSpanSuspectData> newSuspects = await FindSuspectsForSpanAsync(streamConfig, span.Fingerprint, job.Change + 1, span.FirstFailure.Change);

					if (await _issueCollection.TryUpdateSpanAsync(span, newLastSuccess: newLastSuccess, newSuspects: newSuspects) == null)
					{
						return false;
					}

					_logger.LogInformation("Set last success for issue {IssueId}, template {TemplateId}, node {Node} as job {JobId}, cl {Change}", span.IssueId, job.TemplateId, span.NodeName, job.Id, job.Change);
					await UpdateIssueDerivedDataAsync(span.IssueId);
				}
				else if (job.Change > span.LastFailure.Change && (span.NextSuccess == null || job.Change < span.NextSuccess.Change))
				{					
					IIssue? issue = await _issueCollection.GetIssueAsync(span.IssueId);
					if (issue == null || issue.QuarantinedByUserId == null)
					{
						NewIssueStepData newNextSucccess = new NewIssueStepData(job.Change, IssueSeverity.Unspecified, job.Name, job.Id, batch.Id, step.Id, step.StartTimeUtc ?? default, step.LogId, null, false);

						if (await _issueCollection.TryUpdateSpanAsync(span, newNextSuccess: newNextSucccess) == null)
						{
							return false;
						}

						_logger.LogInformation("Set next success for issue {IssueId}, template {TemplateId}, node {Node} as job {JobId}, cl {Change}", span.IssueId, job.TemplateId, span.NodeName, job.Id, job.Change);

					}
					else
					{
						// If the issue is quarantined, spans may have been updated by log events, though in the case they aren't
						// need to add a step to the span history
						List<IIssueStep> steps = await _issueCollection.FindStepsAsync(span.Id);
						if (steps.FirstOrDefault(x => x.JobId == job.Id && x.StepId == step.Id) == null)
						{
							NewIssueStepData newStep = new NewIssueStepData(job, batch, step, IssueSeverity.Unspecified, null, false);
							await _issueCollection.AddStepAsync(span.Id, newStep);
							_logger.LogInformation("Adding step to quarantined issue {IssueId}, template {TemplateId}, node {Node} job {JobId} batch {BatchId} step {StepId} cl {Change}", span.IssueId, job.TemplateId, span.NodeName, job.Id, batch.Id, step.Id, job.Change);
						}
					}
					
					await UpdateIssueDerivedDataAsync(span.IssueId);
				}
			}
			return true;
		}

		/// <summary>
		/// Attempts to parse the Robomerge source from this commit information
		/// </summary>
		/// <param name="description">Description text to parse</param>
		/// <returns>The parsed source changelist, or null if no #ROBOMERGE-SOURCE tag was present</returns>
		static int? ParseRobomergeSource(string description)
		{
			// #ROBOMERGE-SOURCE: CL 13232051 in //Fortnite/Release-12.60/... via CL 13232062 via CL 13242953
			Match match = Regex.Match(description, @"^#ROBOMERGE-SOURCE: CL (\d+)", RegexOptions.Multiline);
			if (match.Success)
			{
				return Int32.Parse(match.Groups[1].Value, CultureInfo.InvariantCulture);
			}
			else
			{
				return null;
			}
		}

		/// <summary>
		/// Attempts to parse the Robomerge owner from this commit information
		/// </summary>
		/// <param name="description">Description text to parse</param>
		/// <returns>The Robomerge owner, or null if no #ROBOMERGE-OWNER tag was present</returns>
		static string? ParseRobomergeOwner(string description)
		{
			// #ROBOMERGE-OWNER: ben.marsh
			Match match = Regex.Match(description, @"^#ROBOMERGE-OWNER:\s*([^\s]+)", RegexOptions.Multiline);
			if (match.Success)
			{
				return match.Groups[1].Value;
			}
			else
			{
				return null;
			}
		}

		/// <summary>
		/// Finds the handler for a given issue
		/// </summary>
		/// <param name="fingerprint">The fingerprint to get the handler for</param>
		/// <param name="handler">Receives the handler on success</param>
		/// <returns>True if a matching handler was found</returns>
		bool TryGetHandler(IIssueFingerprint fingerprint, [NotNullWhen(true)] out IssueHandler? handler)
		{
			string type = fingerprint.Type;

			int endIdx = type.IndexOf(':', StringComparison.Ordinal);
			if (endIdx != -1)
			{
				type = type.Substring(0, endIdx);
			}

			return _typeToHandler.TryGetValue(type, out handler);
		}
	}
}
