// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Horde.Build.Users;
using Horde.Build.Logs;
using Horde.Build.Issues;
using Horde.Build.Utilities;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using MongoDB.Bson;

namespace Horde.Build.Ugs
{
	using LogId = ObjectId<ILogFile>;
	using UserId = ObjectId<IUser>;

	/// <summary>
	/// Controller for the /api/v1/issues endpoint
	/// </summary>
	[ApiController]
	[Route("[controller]")]
	public sealed class UgsController : ControllerBase
	{
		/// <summary>
		/// Singleton instance of the issue service
		/// </summary>
		private readonly IssueService _issueService;

		/// <summary>
		/// Collection of metadata documents
		/// </summary>
		private readonly IUgsMetadataCollection _ugsMetadataCollection;

		/// <summary>
		/// Collection of users
		/// </summary>
		private readonly IUserCollection _userCollection;

		/// <summary>
		/// The log file service
		/// </summary>
		private readonly ILogFileService _logFileService;

		/// <summary>
		/// Server settings
		/// </summary>
		private readonly ServerSettings _settings;

		/// <summary>
		/// Logger 
		/// </summary>
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public UgsController(IssueService issueService, IUgsMetadataCollection ugsMetadataCollection, IUserCollection userCollection, ILogFileService logFileService, IOptionsMonitor<ServerSettings> optionsMonitor, ILogger<UgsController> logger)
		{
			_issueService = issueService;
			_ugsMetadataCollection = ugsMetadataCollection;
			_userCollection = userCollection;
			_logFileService = logFileService;
			_settings = optionsMonitor.CurrentValue;
			_logger = logger;
		}

		/// <summary>
		/// Gets the latest version info
		/// </summary>
		/// <returns>Result code</returns>
		[HttpGet]
		[Route("/ugs/api/latest")]
		public ActionResult<object> GetLatest()
		{
			return new { Version = 2 };
		}

		/// <summary>
		/// Adds new metadata to the database
		/// </summary>
		/// <param name="request">Request object</param>
		/// <returns>Result code</returns>
		[HttpPost]
		[Route("/ugs/api/metadata")]
		public async Task<ActionResult> AddMetadataAsync(AddUgsMetadataRequest request)
		{
			IUgsMetadata metadata = await _ugsMetadataCollection.FindOrAddAsync(request.Stream, request.Change, request.Project);
			if (request.Synced != null || request.Vote != null || request.Investigating != null || request.Starred != null || request.Comment != null)
			{
				if (request.UserName == null)
				{
					return BadRequest("Missing UserName field on request body");
				}
				metadata = await _ugsMetadataCollection.UpdateUserAsync(metadata, request.UserName, request.Synced, request.Vote, request.Investigating, request.Starred, request.Comment);
			}
			if (request.Badges != null)
			{
				foreach (AddUgsBadgeRequest badge in request.Badges)
				{
					metadata = await _ugsMetadataCollection.UpdateBadgeAsync(metadata, badge.Name, badge.Url, badge.State);
				}
			}
			return Ok();
		}

		/// <summary>
		/// Searches for metadata updates
		/// </summary>
		/// <param name="stream">THe stream to search for</param>
		/// <param name="minChange">Minimum changelist number</param>
		/// <param name="maxChange">Maximum changelist number</param>
		/// <param name="project">The project identifiers to search for</param>
		/// <param name="sequence">Last sequence number</param>
		/// <returns>List of metadata updates</returns>
		[HttpGet]
		[Route("/ugs/api/metadata")]
		public async Task<GetUgsMetadataListResponse> FindMetadataAsync([FromQuery] string stream, [FromQuery] int minChange, [FromQuery] int? maxChange = null, [FromQuery] string? project = null, [FromQuery] long? sequence = null)
		{
			List<IUgsMetadata> metadataList = await _ugsMetadataCollection.FindAsync(stream, minChange, maxChange, sequence);
		
			GetUgsMetadataListResponse response = new GetUgsMetadataListResponse();
			if(sequence != null)
			{
				response.SequenceNumber = sequence.Value;
			}

			foreach (IUgsMetadata metadata in metadataList)
			{
				if (metadata.UpdateTicks > response.SequenceNumber)
				{
					response.SequenceNumber = metadata.UpdateTicks;
				}
				if (String.IsNullOrEmpty(metadata.Project) || metadata.Project.Equals(project, StringComparison.OrdinalIgnoreCase))
				{
					response.Items.Add(new GetUgsMetadataResponse(metadata));
				}
			}
			return response;
		}

		/// <summary>
		/// Retrieve information about open issues
		/// </summary>
		/// <param name="user"></param>
		/// <param name="includeResolved">Whether to include resolved issues</param>
		/// <param name="maxResults">Maximum number of results to return</param>
		/// <returns>List of matching agents</returns>
		[HttpGet]
		[Route("/ugs/api/issues")]
		[ProducesResponseType(typeof(GetUgsIssueResponse), 200)]
		public async Task<ActionResult<List<GetUgsIssueResponse>>> GetIssuesAsync([FromQuery] string? user = null, [FromQuery] bool includeResolved = false, [FromQuery] int maxResults = 100)
		{
			IUser? userInfo = (user != null) ? await _userCollection.FindUserByLoginAsync(user) : null;

			List<GetUgsIssueResponse> responses = new List<GetUgsIssueResponse>();
			if (includeResolved)
			{
				List<IIssue> issues = await _issueService.Collection.FindIssuesAsync(null, resolved: null, count: maxResults);
				foreach(IIssue issue in issues)
				{
					IIssueDetails details = await _issueService.GetIssueDetailsAsync(issue);
					bool bNotify = userInfo != null && details.Suspects.Any(x => x.AuthorId == userInfo.Id);
					responses.Add(await CreateIssueResponseAsync(details, bNotify));
				}
			}
			else
			{
				foreach (IIssueDetails cachedOpenIssue in _issueService.CachedOpenIssues)
				{
					if (responses.Count >= maxResults)
					{
						break;
					}

					if (cachedOpenIssue.ShowNotifications())
					{
						bool bNotify = userInfo != null && cachedOpenIssue.IncludeForUser(userInfo.Id);
						responses.Add(await CreateIssueResponseAsync(cachedOpenIssue, bNotify));
					}
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
		[Route("/ugs/api/issues/{issueId}")]
		[ProducesResponseType(typeof(GetUgsIssueBuildResponse), 200)]
		public async Task<ActionResult<object>> GetIssueAsync(int issueId, [FromQuery] PropertyFilter? filter = null)
		{
			IIssueDetails? issue = await _issueService.GetIssueDetailsAsync(issueId);
			if (issue == null)
			{
				return NotFound();
			}

			return PropertyFilter.Apply(await CreateIssueResponseAsync(issue, false), filter);
		}

		/// <summary>
		/// Retrieve information about builds for a specific issue
		/// </summary>
		/// <param name="issueId">Id of the issue to get information about</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>List of matching agents</returns>
		[HttpGet]
		[Route("/ugs/api/issues/{issueId}/builds")]
		[ProducesResponseType(typeof(List<GetUgsIssueBuildResponse>), 200)]
		public async Task<ActionResult<List<object>>> GetIssueBuildsAsync(int issueId, [FromQuery] PropertyFilter? filter = null)
		{
			IIssueDetails? issue = await _issueService.GetCachedIssueDetailsAsync(issueId);
			if (issue == null)
			{
				return NotFound();
			}

			List<GetUgsIssueBuildResponse> responses = new List<GetUgsIssueBuildResponse>();
			foreach (IIssueSpan span in issue.Spans)
			{
				if (span.LastSuccess != null)
				{
					responses.Add(CreateBuildResponse(span, span.LastSuccess, IssueBuildOutcome.Success));
				}
				foreach (IIssueStep step in issue.Steps)
				{
					responses.Add(CreateBuildResponse(span, step, IssueBuildOutcome.Error));
				}
				if (span.NextSuccess != null)
				{
					responses.Add(CreateBuildResponse(span, span.NextSuccess, IssueBuildOutcome.Success));
				}
			}

			return responses.ConvertAll(x => PropertyFilter.Apply(x, filter));
		}

		/// <summary>
		/// Retrieve information about builds for a specific issue
		/// </summary>
		/// <param name="issueId">Id of the issue to get information about</param>
		/// <returns>List of matching agents</returns>
		[HttpGet]
		[Route("/ugs/api/issues/{issueId}/diagnostics")]
		[ProducesResponseType(typeof(List<GetUgsIssueDiagnosticResponse>), 200)]
		public async Task<ActionResult<List<GetUgsIssueDiagnosticResponse>>> GetIssueDiagnosticsAsync(int issueId)
		{
			List<GetUgsIssueDiagnosticResponse> diagnostics = new List<GetUgsIssueDiagnosticResponse>();

			Dictionary<LogId, ILogFile?> logFiles = new Dictionary<LogId, ILogFile?>();

			List<IIssueSpan> spans = await _issueService.Collection.FindSpansAsync(issueId);
			List<ILogEvent> events = await _logFileService.FindEventsForSpansAsync(spans.Select(x => x.Id), null, 0, count: 10);

			foreach (ILogEvent logEvent in events)
			{
				ILogFile? logFile;
				if(!logFiles.TryGetValue(logEvent.LogId, out logFile))
				{
					logFile = await _logFileService.GetLogFileAsync(logEvent.LogId);
					logFiles.Add(logEvent.LogId, logFile);
				}
				if (logFile != null)
				{
					ILogEventData eventData = await _logFileService.GetEventDataAsync(logFile, logEvent.LineIndex, logEvent.LineCount);
					long buildId = logEvent.LogId.GetHashCode();
					Uri url = new Uri(_settings.DashboardUrl, $"log/{logEvent.LogId}?lineindex={logEvent.LineIndex}");

					GetUgsIssueDiagnosticResponse diagnostic = new GetUgsIssueDiagnosticResponse(buildId, eventData.Message, url);
					diagnostics.Add(diagnostic);
				}
			}

			return diagnostics;
		}

		/// <summary>
		/// Gets the URL for a failing step in the
		/// </summary>
		/// <param name="details">The issue to get a URL for</param>
		/// <param name="bNotify">Whether to show notifications for this issue</param>
		/// <returns>The issue response</returns>
		async Task<GetUgsIssueResponse> CreateIssueResponseAsync(IIssueDetails details, bool bNotify)
		{
			Uri? buildUrl = GetIssueBuildUrl(details);

			IUser? owner = details.Issue.OwnerId.HasValue ? await _userCollection.GetCachedUserAsync(details.Issue.OwnerId.Value) : null;
			IUser? nominatedBy = details.Issue.NominatedById.HasValue ? await _userCollection.GetCachedUserAsync(details.Issue.NominatedById.Value) : null;

			return new GetUgsIssueResponse(details, owner, nominatedBy, bNotify, buildUrl);
		}

		/// <summary>
		/// Gets the URL for a failing step in the given issue
		/// </summary>
		/// <param name="issue">The issue details</param>
		/// <returns>The build URL</returns>
		Uri? GetIssueBuildUrl(IIssueDetails issue)
		{
			HashSet<ObjectId> unresolvedSpans = new HashSet<ObjectId>(issue.Spans.Where(x => x.NextSuccess == null).Select(x => x.Id));

			IIssueStep? step = issue.Steps.OrderByDescending(x => unresolvedSpans.Contains(x.SpanId)).ThenByDescending(x => x.Change).FirstOrDefault();
			if (step == null)
			{
				return null;
			}

			return new Uri(_settings.DashboardUrl, $"job/{step.JobId}?step={step.StepId}");
		}

		/// <summary>
		/// Creates the response for a particular build
		/// </summary>
		/// <param name="span">Span containing the step</param>
		/// <param name="step">The step to describe</param>
		/// <param name="outcome">Outcome of this step</param>
		/// <returns>Response object</returns>
		GetUgsIssueBuildResponse CreateBuildResponse(IIssueSpan span, IIssueStep step, IssueBuildOutcome outcome)
		{
			GetUgsIssueBuildResponse response = new GetUgsIssueBuildResponse(span.StreamName, step.Change, outcome);
			response.Id = step.LogId.GetHashCode();
			response.JobName = $"{step.JobName}: {span.NodeName}";
			response.JobUrl = new Uri(_settings.DashboardUrl, $"job/{step.JobId}");
			response.JobStepName = span.NodeName;
			response.JobStepUrl = new Uri(_settings.DashboardUrl, $"job/{step.JobId}?step={step.StepId}");
			response.ErrorUrl = response.JobStepUrl;
			return response;
		}

		/// <summary>
		/// Update an issue
		/// </summary>
		/// <param name="issueId">Id of the issue to get information about</param>
		/// <param name="request">The update information</param>
		/// <returns>List of matching agents</returns>
		[HttpPut]
		[Route("/ugs/api/issues/{issueId}")]
		public async Task<ActionResult> UpdateIssueAsync(int issueId, [FromBody] UpdateUgsIssueRequest request)
		{
			UserId? newOwnerId = null;
			if (!String.IsNullOrEmpty(request.Owner))
			{
				newOwnerId = (await _userCollection.FindOrAddUserByLoginAsync(request.Owner))?.Id;
			}

			UserId? newNominatedById = null;
			if (!String.IsNullOrEmpty(request.NominatedBy))
			{
				newNominatedById = (await _userCollection.FindOrAddUserByLoginAsync(request.NominatedBy))?.Id;
			}

			UserId? newDeclinedById = null;
			if (!String.IsNullOrEmpty(request.DeclinedBy))
			{
				newDeclinedById = (await _userCollection.FindOrAddUserByLoginAsync(request.DeclinedBy))?.Id;
			}

			UserId? newResolvedById = null;
			if (!String.IsNullOrEmpty(request.ResolvedBy))
			{
				newResolvedById = (await _userCollection.FindOrAddUserByLoginAsync(request.ResolvedBy))?.Id;
			}
			if (newResolvedById == null && request.Resolved.HasValue)
			{
				newResolvedById = request.Resolved.Value ? IIssue.ResolvedByUnknownId : UserId.Empty;
			}

			if (!await _issueService.UpdateIssueAsync(issueId, ownerId: newOwnerId, nominatedById: newNominatedById, acknowledged: request.Acknowledged, declinedById: newDeclinedById, fixChange: request.FixChange, resolvedById: newResolvedById))
			{
				return NotFound();
			}
			return Ok();
		}

		/// <summary>
		/// Post information about net core installation. 
		/// </summary>
		[HttpPost]
		[Route("/ugs/api/netcore")]
		public ActionResult<object> PostNetCoreInfo(string? user = null, string? machine = null, bool netCore = false)
		{
			_logger.LogInformation("NetCore: User={User}, Machine={Machine}, NetCore={NetCore}", user, machine, netCore);
			return new { };
		}
	}
}
