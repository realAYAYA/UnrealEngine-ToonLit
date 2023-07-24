// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Horde.Build.Acls;
using Horde.Build.Auditing;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using MongoDB.Bson;
using Microsoft.Extensions.Logging;
using Horde.Build.Jobs;
using Horde.Build.Logs;
using Horde.Build.Streams;
using Horde.Build.Issues.External;
using Horde.Build.Users;
using Horde.Build.Jobs.Graphs;
using Microsoft.Extensions.Options;
using Horde.Build.Server;

namespace Horde.Build.Issues
{
	using JobId = ObjectId<IJob>;
	using LogId = ObjectId<ILogFile>;
	using StreamId = StringId<IStream>;
	using UserId = ObjectId<IUser>;
	using WorkflowId = StringId<WorkflowConfig>;

	/// <summary>
	/// Controller for the /api/v1/issues endpoint
	/// </summary>
	[Authorize]
	[ApiController]
	[Route("[controller]")]
	public class IssuesController : HordeControllerBase
	{
		private readonly AclService _aclService;
		private readonly IIssueCollection _issueCollection;
		private readonly IssueService _issueService;
		private readonly IExternalIssueService _externalIssueService;
		private readonly JobService _jobService;
		private readonly IStreamCollection _streamCollection;
		private readonly IUserCollection _userCollection;
		private readonly ILogFileService _logFileService;
		private readonly IOptionsSnapshot<GlobalConfig> _globalConfig;
		private readonly ILogger<IssuesController> _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public IssuesController(AclService aclService, IIssueCollection issueCollection, IssueService issueService, JobService jobService, IStreamCollection streamCollection, IUserCollection userCollection, ILogFileService logFileService, IExternalIssueService externalIssueService, IOptionsSnapshot<GlobalConfig> globalConfig, ILogger<IssuesController> logger)
		{
			_aclService = aclService;
			_issueCollection = issueCollection;
			_issueService = issueService;
			_jobService = jobService;
			_streamCollection = streamCollection;
			_userCollection = userCollection;
			_logFileService = logFileService;
			_externalIssueService = externalIssueService;
			_globalConfig = globalConfig;
			_logger = logger;
		}

		/// <summary>
		/// Retrieve information about a specific issue
		/// </summary>
		/// <param name="ids">Set of issue ids to find</param>
		/// <param name="streamId">The stream to query for</param>
		/// <param name="minChange">The minimum changelist range to query, inclusive</param>
		/// <param name="maxChange">The minimum changelist range to query, inclusive</param>
		/// <param name="resolved">Whether to include resolved issues</param>
		/// <param name="index">Starting offset of the window of results to return</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>List of matching agents</returns>
		[HttpGet]
		[Route("/api/v2/issues")]
		[ProducesResponseType(typeof(List<FindIssueResponse>), 200)]
		public async Task<ActionResult<object>> FindIssuesV2Async([FromQuery(Name = "Id")] int[]? ids = null, [FromQuery] StreamId? streamId = null, [FromQuery] int? minChange = null, [FromQuery] int? maxChange = null, [FromQuery] bool? resolved = null, [FromQuery] int index = 0, [FromQuery] int count = 10, [FromQuery] PropertyFilter? filter = null)
		{
			if (ids != null && ids.Length == 0)
			{
				ids = null;
			}

			List<object> responses = new List<object>();
			if (streamId != null)
			{
				GlobalConfig globalConfig = _globalConfig.Value;

				StreamConfig? streamConfig;
				if (!globalConfig.TryGetStream(streamId.Value, out streamConfig))
				{
					return NotFound(streamId.Value);
				}
				if (!streamConfig.Authorize(AclAction.ViewStream, User))
				{
					return Forbid(AclAction.ViewStream, streamId.Value);
				}

				List<IIssueSpan> spans = await _issueCollection.FindSpansAsync(null, ids, streamId.Value, minChange, maxChange, resolved);
				if(spans.Count > 0)
				{
					// Group all the spans by their issue id
					Dictionary<int, List<IIssueSpan>> issueIdToSpans = new Dictionary<int, List<IIssueSpan>>();
					foreach (IIssueSpan span in spans)
					{
						List<IIssueSpan>? spansForIssue;
						if (!issueIdToSpans.TryGetValue(span.IssueId, out spansForIssue))
						{
							spansForIssue = new List<IIssueSpan>();
							issueIdToSpans.Add(span.IssueId, spansForIssue);
						}
						spansForIssue.Add(span);
					}

					// Find the matching issues
					List<IIssue> issues = await _issueCollection.FindIssuesAsync(issueIdToSpans.Keys, index: index, count: count);

					// Create the corresponding responses
					foreach (IIssue issue in issues.OrderByDescending(x => x.Id))
					{
						IssueSeverity streamSeverity = IssueSeverity.Unspecified;

						HashSet<WorkflowId> openWorkflowIds = new HashSet<WorkflowId>();

						List<FindIssueSpanResponse> spanResponses = new List<FindIssueSpanResponse>();
						if (issueIdToSpans.TryGetValue(issue.Id, out List<IIssueSpan>? spansForIssue))
						{
							// Find the current severity in the stream
							DateTime lastStepTime = DateTime.MinValue;
							foreach (IIssueSpan span in spansForIssue)
							{
								if (span.LastFailure != null && span.LastFailure.StepTime > lastStepTime)
								{
									lastStepTime = span.LastFailure.StepTime;
									streamSeverity = span.LastFailure.Severity;
								}
							}

							// Convert each issue to a response
							foreach (IIssueSpan span in spansForIssue)
							{
								spanResponses.Add(new FindIssueSpanResponse(span, span.LastFailure.Annotations.WorkflowId));
							}

							// Find which workflows are affected
							foreach (IIssueSpan span in spansForIssue)
							{
								WorkflowId? workflowId = span.LastFailure.Annotations.WorkflowId;
								if (workflowId != null && span.NextSuccess == null && span.StreamId == streamId)
								{
									openWorkflowIds.Add(workflowId.Value);
								}
							}
						}
					
						IUser? owner = null;
						IUser? nominatedBy = null;
						IUser? resolvedBy = null;
						IUser? quarantinedBy = null;

						if (issue.OwnerId != null)
						{
							owner = await _userCollection.GetCachedUserAsync(issue.OwnerId.Value);
						}
						
						if (issue.NominatedById != null)
						{
							nominatedBy = await _userCollection.GetCachedUserAsync(issue.NominatedById.Value);
						}
						
						if (issue.ResolvedById != null)
						{
							resolvedBy = await _userCollection.GetCachedUserAsync(issue.ResolvedById.Value);
						}

						if (issue.QuarantinedByUserId != null)
						{
							quarantinedBy = await _userCollection.GetCachedUserAsync(issue.QuarantinedByUserId.Value);
						}

						FindIssueResponse response = new FindIssueResponse(issue, owner, nominatedBy, resolvedBy, quarantinedBy, streamSeverity, spanResponses, openWorkflowIds.ToList());
						responses.Add(PropertyFilter.Apply(response, filter));
					}
				}
			}
			else
			{
				return BadRequest("Missing StreamId on request");
			}

			return responses;
		}

		/// <summary>
		/// Retrieve information about a specific issue
		/// </summary>
		/// <param name="ids">Set of issue ids to find</param>
		/// <param name="streamId">The stream to query for</param>
		/// <param name="change">The changelist to query</param>
		/// <param name="minChange">The minimum changelist range to query, inclusive</param>
		/// <param name="maxChange">The minimum changelist range to query, inclusive</param>
		/// <param name="jobId">Job id to filter by</param>
		/// <param name="batchId">The batch to filter by</param>
		/// <param name="stepId">The step to filter by</param>
		/// <param name="labelIdx">The label within the job to filter by</param>
		/// <param name="ownerId">User to filter issues for</param>
		/// <param name="resolved">Whether to include resolved issues</param>
		/// <param name="promoted">Whether to include promoted issues</param>
		/// <param name="index">Starting offset of the window of results to return</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>List of matching agents</returns>
		[HttpGet]
		[Route("/api/v1/issues")]
		[ProducesResponseType(typeof(List<GetIssueResponse>), 200)]
		public async Task<ActionResult<object>> FindIssuesAsync([FromQuery(Name = "Id")] int[]? ids = null, [FromQuery] string? streamId = null, [FromQuery] int? change = null, [FromQuery] int? minChange = null, [FromQuery] int? maxChange = null, [FromQuery] JobId? jobId = null, [FromQuery] string? batchId = null, [FromQuery] string? stepId = null, [FromQuery(Name = "label")] int? labelIdx = null, [FromQuery] string? ownerId = null, [FromQuery] bool? resolved = null, [FromQuery] bool? promoted = null, [FromQuery] int index = 0, [FromQuery] int count = 10, [FromQuery] PropertyFilter? filter = null)
		{
			if(ids != null && ids.Length == 0)
			{
				ids = null;
			}

			UserId? ownerIdValue = null;
			if (ownerId != null)
			{
				ownerIdValue = new UserId(ownerId);
			}

			List<IIssue> issues;
			if (jobId == null)
			{
				StreamId? streamIdValue = null;
				if (streamId != null)
				{
					streamIdValue = new StreamId(streamId);
				}

				issues = await _issueService.Collection.FindIssuesAsync(ids, ownerIdValue, streamIdValue, minChange ?? change, maxChange ?? change, resolved, promoted, index, count);
			}
			else
			{
				IJob? job = await _jobService.GetJobAsync(jobId.Value);
				if (job == null)
				{
					return NotFound(jobId.Value);
				}
				if(!_globalConfig.Value.Authorize(job, AclAction.ViewJob, User))
				{
					return Forbid(AclAction.ViewJob, jobId.Value);
				}

				IGraph graph = await _jobService.GetGraphAsync(job);
				issues = await _issueService.Collection.FindIssuesForJobAsync(job, graph, stepId?.ToSubResourceId(), batchId?.ToSubResourceId(), labelIdx, ownerIdValue, resolved, promoted, index, count);
			}

			List<object> responses = new List<object>();
			foreach (IIssue issue in issues)
			{
				IIssueDetails details = await _issueService.GetIssueDetailsAsync(issue);
				if (AuthorizeIssue(details))
				{
					bool showDesktopAlerts = _issueService.ShowDesktopAlertsForIssue(issue, details.Spans);
					GetIssueResponse response = CreateIssueResponse(details, showDesktopAlerts);
					responses.Add(PropertyFilter.Apply(response, filter));
				}
			}
			return responses;
		}

		/// <summary>
		/// Retrieve information about a specific issue
		/// </summary>
		/// <param name="issueId">Id of the issue to get information about</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>List of matching agents</returns>
		[HttpGet]
		[Route("/api/v1/issues/{issueId}")]
		[ProducesResponseType(typeof(GetIssueResponse), 200)]
		public async Task<ActionResult<object>> GetIssueAsync(int issueId, [FromQuery] PropertyFilter? filter = null)
		{
			IIssueDetails? details = await _issueService.GetIssueDetailsAsync(issueId);
			if (details == null)
			{
				return NotFound();
			}
			if (!AuthorizeIssue(details))
			{
				return Forbid();
			}

			bool showDesktopAlerts = _issueService.ShowDesktopAlertsForIssue(details.Issue, details.Spans);
			return PropertyFilter.Apply(CreateIssueResponse(details, showDesktopAlerts), filter);
		}

		/// <summary>
		/// Retrieve historical information about a specific issue
		/// </summary>
		/// <param name="issueId">Id of the agent to get information about</param>
		/// <param name="minTime">Minimum time for records to return</param>
		/// <param name="maxTime">Maximum time for records to return</param>
		/// <param name="index">Offset of the first result</param>
		/// <param name="count">Number of records to return</param>
		/// <returns>Information about the requested agent</returns>
		[HttpGet]
		[Route("/api/v1/issues/{issueId}/history")]
		public async Task GetAgentHistoryAsync(int issueId, [FromQuery] DateTime? minTime = null, [FromQuery] DateTime? maxTime = null, [FromQuery] int index = 0, [FromQuery] int count = 50)
		{
			Response.ContentType = "application/json";
			Response.StatusCode = 200;
			await Response.StartAsync();
			await _issueCollection.GetLogger(issueId).FindAsync(Response.BodyWriter, minTime, maxTime, index, count);
		}

		/// <summary>
		/// Create an issue response object
		/// </summary>
		/// <param name="details"></param>
		/// <param name="showDesktopAlerts"></param>
		/// <returns></returns>
		GetIssueResponse CreateIssueResponse(IIssueDetails details, bool showDesktopAlerts)
		{
			List<GetIssueAffectedStreamResponse> affectedStreams = new List<GetIssueAffectedStreamResponse>();
			foreach (IGrouping<StreamId, IIssueSpan> streamSpans in details.Spans.GroupBy(x => x.StreamId))
			{
				try
				{
					_globalConfig.Value.TryGetStream(streamSpans.Key, out StreamConfig? streamConfig);
					affectedStreams.Add(new GetIssueAffectedStreamResponse(details, streamConfig, streamSpans));
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Unable to get {StreamId} for span key on issue {IssueId}", streamSpans.Key, details.Issue.Id);
				}
			}
			return new GetIssueResponse(details, affectedStreams, showDesktopAlerts);
		}

		/// <summary>
		/// Retrieve events for a specific issue
		/// </summary>
		/// <param name="issueId">Id of the issue to get information about</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>List of matching agents</returns>
		[HttpGet]
		[Route("/api/v1/issues/{issueId}/streams")]
		[ProducesResponseType(typeof(List<GetIssueStreamResponse>), 200)]
		public async Task<ActionResult<List<object>>> GetIssueStreamsAsync(int issueId, [FromQuery] PropertyFilter? filter = null)
		{
			IIssueDetails? issue = await _issueService.GetIssueDetailsAsync(issueId);
			if (issue == null)
			{
				return NotFound();
			}
			if (!AuthorizeIssue(issue))
			{
				return Forbid();
			}

			List<object> responses = new List<object>();
			foreach (IGrouping<StreamId, IIssueSpan> spanGroup in issue.Spans.GroupBy(x => x.StreamId))
			{
				if (_globalConfig.Value.TryGetStream(spanGroup.Key, out StreamConfig? streamConfig) && streamConfig.Authorize(AclAction.ViewStream, User))
				{
					HashSet<ObjectId> spanIds = new HashSet<ObjectId>(spanGroup.Select(x => x.Id));
					List<IIssueStep> steps = issue.Steps.Where(x => spanIds.Contains(x.SpanId)).ToList();
					responses.Add(PropertyFilter.Apply(new GetIssueStreamResponse(spanGroup.Key, spanGroup.ToList(), steps), filter));
				}
			}
			return responses;
		}

		/// <summary>
		/// Retrieve events for a specific issue
		/// </summary>
		/// <param name="issueId">Id of the issue to get information about</param>
		/// <param name="streamId">The stream id</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>List of matching agents</returns>
		[HttpGet]
		[Route("/api/v1/issues/{issueId}/streams/{streamId}")]
		[ProducesResponseType(typeof(List<GetIssueStreamResponse>), 200)]
		public async Task<ActionResult<object>> GetIssueStreamAsync(int issueId, StreamId streamId, [FromQuery] PropertyFilter? filter = null)
		{
			IIssueDetails? details = await _issueService.GetIssueDetailsAsync(issueId);
			if (details == null)
			{
				return NotFound();
			}
			if (!_globalConfig.Value.TryGetStream(streamId, out StreamConfig? streamConfig))
			{
				return NotFound(streamId);
			}
			if (!streamConfig.Authorize(AclAction.ViewStream, User))
			{
				return Forbid(AclAction.ViewStream, streamId);
			}

			List<IIssueSpan> spans = details.Spans.Where(x => x.StreamId == streamId).ToList();
			if(spans.Count == 0)
			{
				return NotFound();
			}

			HashSet<ObjectId> spanIds = new HashSet<ObjectId>(spans.Select(x => x.Id));
			List<IIssueStep> steps = details.Steps.Where(x => spanIds.Contains(x.SpanId)).ToList();

			return PropertyFilter.Apply(new GetIssueStreamResponse(streamId, spans, steps), filter);
		}

		/// <summary>
		/// Retrieve events for a specific issue
		/// </summary>
		/// <param name="issueId">Id of the issue to get information about</param>
		/// <param name="jobId">The job id to filter for</param>
		/// <param name="batchId">The batch to filter by</param>
		/// <param name="stepId">The step to filter by</param>
		/// <param name="labelIdx">The label within the job to filter by</param>
		/// <param name="logIds">List of log ids to return issues for</param>
		/// <param name="index">Index of the first event</param>
		/// <param name="count">Number of events to return</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <param name="cancellationToken">Cancellation token for the request</param>
		/// <returns>List of matching agents</returns>
		[HttpGet]
		[Route("/api/v1/issues/{issueId}/events")]
		[ProducesResponseType(typeof(List<GetLogEventResponse>), 200)]
		public async Task<ActionResult<List<object>>> GetIssueEventsAsync(
			int issueId,
			[FromQuery] JobId? jobId = null,
			[FromQuery] string? batchId = null,
			[FromQuery] string? stepId = null,
			[FromQuery(Name = "label")] int? labelIdx = null,
			[FromQuery] string[]? logIds = null,
			[FromQuery] int index = 0, 
			[FromQuery] int count = 10,
			[FromQuery] PropertyFilter? filter = null,
			CancellationToken cancellationToken = default)
		{
			HashSet<LogId> logIdValues = new HashSet<LogId>();
			if(jobId != null)
			{
				IJob? job = await _jobService.GetJobAsync(jobId.Value);
				if(job == null)
				{
					return NotFound();
				}

				if (stepId != null)
				{
					IJobStep? step;
					if (job.TryGetStep(stepId.ToSubResourceId(), out step) && step.Outcome != JobStepOutcome.Success && step.LogId != null)
					{
						logIdValues.Add(step.LogId.Value);
					}
				}
				else if (batchId != null)
				{
					IJobStepBatch? batch;
					if (job.TryGetBatch(batchId.ToSubResourceId(), out batch))
					{
						logIdValues.UnionWith(batch.Steps.Where(x => x.Outcome != JobStepOutcome.Success && x.LogId != null).Select(x => x.LogId!.Value));
					}
				}
				else if (labelIdx != null)
				{
					IGraph graph = await _jobService.GetGraphAsync(job);

					HashSet<NodeRef> includedNodes = new HashSet<NodeRef>(graph.Labels[labelIdx.Value].IncludedNodes);

					foreach (IJobStepBatch batch in job.Batches)
					{
						foreach (IJobStep step in batch.Steps)
						{
							NodeRef nodeRef = new NodeRef(batch.GroupIdx, step.NodeIdx);
							if (step.Outcome != JobStepOutcome.Success && step.LogId != null && includedNodes.Contains(nodeRef))
							{
								logIdValues.Add(step.LogId.Value);
							}
						}
					}
				}
				else
				{
					logIdValues.UnionWith(job.Batches.SelectMany(x => x.Steps).Where(x => x.Outcome != JobStepOutcome.Success && x.LogId != null).Select(x => x.LogId!.Value));
				}
			}
			if(logIds != null)
			{
				logIdValues.UnionWith(logIds.Select(x => new LogId(x)));
			}

			List<IIssueSpan> spans = await _issueCollection.FindSpansAsync(issueId);
			List<ILogEvent> events = await _logFileService.FindEventsForSpansAsync(spans.Select(x => x.Id), logIdValues.ToArray(), index, count, cancellationToken);

			JobPermissionsCache permissionsCache = new JobPermissionsCache();
			Dictionary<LogId, ILogFile?> logFiles = new Dictionary<LogId, ILogFile?>();

			List<object> responses = new List<object>();
			foreach (ILogEvent logEvent in events)
			{
				ILogFile? logFile;
				if (!logFiles.TryGetValue(logEvent.LogId, out logFile))
				{
					logFile = await _logFileService.GetLogFileAsync(logEvent.LogId, cancellationToken);
					logFiles[logEvent.LogId] = logFile;
				}
				if (logFile != null && await _jobService.AuthorizeAsync(logFile.JobId, AclAction.ViewLog, User, _globalConfig.Value))
				{
					ILogEventData data = await _logFileService.GetEventDataAsync(logFile, logEvent.LineIndex, logEvent.LineCount, cancellationToken);
					GetLogEventResponse response = new GetLogEventResponse(logEvent, data, issueId);
					responses.Add(PropertyFilter.Apply(response, filter));
				}
			}
			return responses;
		}

		/// <summary>
		/// Authorize the current user to see an issue
		/// </summary>
		/// <param name="issue">The issue to authorize</param>
		/// <returns>True if the user is authorized to see the issue</returns>
		private bool AuthorizeIssue(IIssueDetails issue)
		{
			foreach (StreamId streamId in issue.Spans.Select(x => x.StreamId).Distinct())
			{
				if (_globalConfig.Value.TryGetStream(streamId, out StreamConfig? streamConfig) && streamConfig.Authorize(AclAction.ViewStream, User))
				{
					return true;
				}
			}
			return false;
		}

		/// <summary>
		/// Update an issue
		/// </summary>
		/// <param name="issueId">Id of the issue to get information about</param>
		/// <param name="request">The update information</param>
		/// <returns>List of matching agents</returns>
		[HttpPut]
		[Route("/api/v1/issues/{issueId}")]
		public async Task<ActionResult> UpdateIssueAsync(int issueId, [FromBody] UpdateIssueRequest request)
		{
			UserId? newOwnerId = null;
			if (request.OwnerId != null)
			{
				newOwnerId = request.OwnerId.Length == 0 ? UserId.Empty : new UserId(request.OwnerId);
			}

			UserId? newNominatedById = null;
			if (request.NominatedById != null)
			{
				newNominatedById = new UserId(request.NominatedById);
			}
			else if (request.OwnerId != null)
			{
				newNominatedById = User.GetUserId();
			}

			UserId? newDeclinedById = null;
			if (request.Declined ?? false)
			{
				newDeclinedById = User.GetUserId();
			}

			UserId? newResolvedById = null;
			if (request.Resolved.HasValue)
			{
				newResolvedById = request.Resolved.Value ? User.GetUserId() : UserId.Empty;
			}

			UserId? newQuarantinedById = null;
			if (request.QuarantinedById != null)
			{
				newQuarantinedById = request.QuarantinedById.Length > 0 ? new UserId(request.QuarantinedById) : UserId.Empty;
			}

			UserId? newForceClosedById = null;
			if (request.ForceClosedById != null)
			{
				newForceClosedById = request.ForceClosedById.Length > 0 ? new UserId(request.ForceClosedById) : UserId.Empty;
			}


			List<ObjectId>? addSpans = null;
			if (request.AddSpans != null && request.AddSpans.Count > 0)
			{
				addSpans = request.AddSpans.ConvertAll(x => ObjectId.Parse(x));
			}

			List<ObjectId>? removeSpans = null;
			if (request.RemoveSpans != null && request.RemoveSpans.Count > 0)
			{
				removeSpans = request.RemoveSpans.ConvertAll(x => ObjectId.Parse(x));
			}

			if (!await _issueService.UpdateIssueAsync(issueId, request.Summary, request.Description, request.Promoted, newOwnerId, newNominatedById, request.Acknowledged, newDeclinedById, request.FixChange, newResolvedById, addSpans, removeSpans, request.ExternalIssueKey, newQuarantinedById, newForceClosedById, initiatedById: User.GetUserId()))
			{
				return NotFound();
			}
			return Ok();
		}

		// External issue tracking

		/// <summary>
		/// Get external issue information for provided keys
		/// </summary>
		[HttpGet]
		[Authorize]
		[Route("/api/v1/issues/external")]
		public async Task<ActionResult<List<GetExternalIssueResponse>>> GetExternalIssuesAsync([FromQuery] StreamId streamId, [FromQuery] string[] keys)
		{
			if (_externalIssueService == null)
			{
				return NotFound();
			}
			if (!_globalConfig.Value.TryGetStream(streamId, out StreamConfig? streamConfig))
			{
				return NotFound(streamId);
			}
			if (!streamConfig.Authorize(AclAction.ViewStream, User))
			{
				return Forbid(AclAction.ViewStream, streamId);
			}

			List<GetExternalIssueResponse> response = new List<GetExternalIssueResponse>();

			if (keys.Length != 0)
			{
				List<IExternalIssue> issues = await _externalIssueService.GetIssuesAsync(keys);

				for (int i = 0; i < issues.Count; i++)
				{
					response.Add(new GetExternalIssueResponse(issues[i]));
				}
			}

			return response;
		}

		/// <summary>
		/// Create a new external issue
		/// </summary>
		[HttpPost]
		[Authorize]
		[Route("/api/v1/issues/external")]
		public async Task<ActionResult<CreateExternalIssueResponse>> CreateExternalIssueAsync([FromBody] CreateExternalIssueRequest issueRequest)
		{
			if (_externalIssueService == null)
			{
				return NotFound();
			}

			StreamId streamIdValue = new StreamId(issueRequest.StreamId);
			if (!_globalConfig.Value.Authorize(AclAction.ViewStream, User))
			{
				return Forbid(AclAction.ViewStream, streamIdValue);
			}

			IUser? user = await _userCollection.GetUserAsync(User);

			if (user == null)
			{
				return BadRequest($"Missing user for {User.GetUserName()}");
			}

			(string? key, string? url) = await _externalIssueService.CreateIssueAsync(user, User.GetExternalIssueUser(), issueRequest.IssueId, issueRequest.Summary, issueRequest.ProjectId, issueRequest.ComponentId, issueRequest.IssueTypeId, issueRequest.Description, issueRequest.HordeIssueLink);

			if (key == null)
			{
				throw new Exception($"Unable to create external issue");
			}

			return new CreateExternalIssueResponse(key, url);
		}

		/// <summary>
		/// Gets external issue tracking projects associated with the provided stream
		/// </summary>
		/// <param name="streamId"></param>
		/// <returns></returns>
		[HttpGet]
		[Authorize]
		[Route("/api/v1/issues/external/projects")]
		public async Task<ActionResult<List<GetExternalIssueProjectResponse>>> GetExternalIssueProjectsAsync([FromQuery] string streamId)
		{
			if (_externalIssueService == null)
			{
				return NotFound();
			}

			StreamId streamIdValue = new StreamId(streamId);
			if (!_globalConfig.Value.TryGetStream(streamIdValue, out StreamConfig? streamConfig))
			{
				return NotFound(streamIdValue);
			}
			if (!streamConfig.Authorize(AclAction.ViewStream, User))
			{
				return Forbid(AclAction.ViewStream, streamIdValue);
			}

			List<IExternalIssueProject> projects = await _externalIssueService.GetProjects(streamConfig);
			List<GetExternalIssueProjectResponse> response = new List<GetExternalIssueProjectResponse>();

			projects.ForEach(project =>
			{
				response.Add(new GetExternalIssueProjectResponse(project.Key, project.Name, project.Id, project.Components, project.IssueTypes));
			});

			return response;
		}
	}
}
