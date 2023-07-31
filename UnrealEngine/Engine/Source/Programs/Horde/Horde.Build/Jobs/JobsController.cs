// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Perforce;
using Horde.Build.Acls;
using Horde.Build.Agents;
using Horde.Build.Jobs.Artifacts;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Jobs.Templates;
using Horde.Build.Jobs.Timing;
using Horde.Build.Logs;
using Horde.Build.Notifications;
using Horde.Build.Perforce;
using Horde.Build.Streams;
using Horde.Build.Users;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Logging;
using MongoDB.Bson;
using OpenTracing;
using OpenTracing.Util;

namespace Horde.Build.Jobs
{
	using JobId = ObjectId<IJob>;
	using StreamId = StringId<IStream>;
	using TemplateRefId = StringId<TemplateRef>;
	using UserId = ObjectId<IUser>;

	/// <summary>
	/// Controller for the /api/v1/jobs endpoing
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class JobsController : HordeControllerBase
	{
		private readonly IGraphCollection _graphs;
		private readonly IPerforceService _perforce;
		private readonly StreamService _streamService;
		private readonly JobService _jobService;
		private readonly ITemplateCollection _templateCollection;
		private readonly IArtifactCollection _artifactCollection;
		private readonly IUserCollection _userCollection;
		private readonly INotificationService _notificationService;
		private readonly AgentService _agentService;
		private readonly ILogger<JobsController> _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public JobsController(IGraphCollection graphs, IPerforceService perforce, StreamService streamService, JobService jobService, ITemplateCollection templateCollection, IArtifactCollection artifactCollection, IUserCollection userCollection, INotificationService notificationService, AgentService agentService, ILogger<JobsController> logger)
		{
			_graphs = graphs;
			_perforce = perforce;
			_streamService = streamService;
			_jobService = jobService;
			_templateCollection = templateCollection;
			_artifactCollection = artifactCollection;
			_userCollection = userCollection;
			_notificationService = notificationService;
			_agentService = agentService;
			_logger = logger;
		}

		/// <summary>
		/// Creates a new job
		/// </summary>
		/// <param name="create">Properties of the new job</param>
		/// <returns>Id of the new job</returns>
		[HttpPost]
		[Route("/api/v1/jobs")]
		public async Task<ActionResult<CreateJobResponse>> CreateJobAsync([FromBody] CreateJobRequest create)
		{
			IStream? stream = await _streamService.GetStreamAsync(new StreamId(create.StreamId));
			if (stream == null)
			{
				return NotFound(create.StreamId);
			}
			if (!await _streamService.AuthorizeAsync(stream, AclAction.CreateJob, User, null))
			{
				return Forbid(AclAction.CreateJob, stream.Id);
			}

			// Get the name of the template ref
			TemplateRefId templateRefId = new TemplateRefId(create.TemplateId);

			// Augment the request with template properties
			TemplateRef? templateRef;
			if (!stream.Templates.TryGetValue(templateRefId, out templateRef))
			{
				return BadRequest($"Template {create.TemplateId} is not available for stream {stream.Id}");
			}
			if (!await _streamService.AuthorizeAsync(stream, templateRef, AclAction.CreateJob, User, null))
			{
				return Forbid(AclAction.CreateJob, stream.Id);
			}

			ITemplate? template = await _templateCollection.GetAsync(templateRef.Hash);
			if (template == null)
			{
				return BadRequest("Missing template referenced by {TemplateId}", create.TemplateId);
			}
			if (!template.AllowPreflights && create.PreflightChange > 0)
			{
				return BadRequest("Template {TemplateId} does not allow preflights", create.TemplateId);
			}

			// Get the name of the new job
			string name = create.Name ?? template.Name;
			if (create.TemplateId.Equals("stage-to-marketplace", StringComparison.Ordinal) && create.Arguments != null)
			{
				foreach (string argument in create.Arguments)
				{
					const string Prefix = "-set:UserContentItems=";
					if (argument.StartsWith(Prefix, StringComparison.Ordinal))
					{
						name += $" - {argument.Substring(Prefix.Length)}";
						break;
					}
				}
			}

			// Get the priority of the new job
			Priority priority = create.Priority ?? template.Priority ?? Priority.Normal;

			// New groups for the job
			IGraph graph = await _graphs.AddAsync(template);

			// Get the change to build
			int change;
			if (create.Change.HasValue)
			{
				change = create.Change.Value;
			}
			else if (create.ChangeQuery != null)
			{
				change = await ExecuteChangeQueryAsync(stream, new TemplateRefId(create.ChangeQuery.TemplateId ?? create.TemplateId), create.ChangeQuery.Target, create.ChangeQuery.Outcomes ?? new List<JobStepOutcome> { JobStepOutcome.Success });
			}
			else if (create.PreflightChange == null && template.SubmitNewChange != null)
			{
				change = await _perforce.CreateNewChangeForTemplateAsync(stream, template);
			}
			else
			{
				change = await _perforce.GetLatestChangeAsync(stream.ClusterName, stream.Name, null);
			}

			// And get the matching code changelist
			int codeChange = await _perforce.GetCodeChangeAsync(stream.ClusterName, stream.Name, change);

			// New properties for the job
			List<string> arguments = create.Arguments ?? template.GetDefaultArguments();

			// Check the preflight change is valid
			string? preflightDescription = null;
			if (create.PreflightChange != null)
			{
				(CheckShelfResult result, preflightDescription) = await _perforce.CheckShelfAsync(stream.ClusterName, stream.Name, create.PreflightChange.Value, null);
				switch (result)
				{
					case CheckShelfResult.Ok:
						break;
					case CheckShelfResult.NoChange:
						return BadRequest(KnownLogEvents.Horde_InvalidPreflight, "CL {Change} does not exist", create.PreflightChange);
					case CheckShelfResult.NoShelvedFiles:
						return BadRequest(KnownLogEvents.Horde_InvalidPreflight, "CL {Change} does not contain any shelved files", create.PreflightChange);
					case CheckShelfResult.WrongStream:
						return BadRequest(KnownLogEvents.Horde_InvalidPreflight, "CL {Change} does not contain files in {Stream}", create.PreflightChange, stream.Name);
					case CheckShelfResult.MixedStream:
						return BadRequest(KnownLogEvents.Horde_InvalidPreflight, "CL {Change} contains files from multiple streams", create.PreflightChange);
					default:
						return BadRequest(KnownLogEvents.Horde_InvalidPreflight, "CL {Change} cannot be preflighted ({Result})", create.PreflightChange, result);
				}
			}

			bool? updateIssues = null;
			if (template.UpdateIssues)
			{
				updateIssues = true;
			}
			else if (create.UpdateIssues.HasValue)
			{
				updateIssues = create.UpdateIssues.Value;
			}

			// Create the job
			IJob job = await _jobService.CreateJobAsync(null, stream, templateRefId, template.Id, graph, name, change, codeChange, create.PreflightChange, null, preflightDescription, User.GetUserId(), priority, create.AutoSubmit, updateIssues, false, templateRef.ChainedJobs, templateRef.ShowUgsBadges, templateRef.ShowUgsAlerts, templateRef.NotificationChannel, templateRef.NotificationChannelFilter, arguments);
			await UpdateNotificationsAsync(job.Id, new UpdateNotificationsRequest { Slack = true });
			return new CreateJobResponse(job.Id.ToString());
		}

		/// <summary>
		/// Evaluate a change query to determine which CL to run a job at
		/// </summary>
		/// <param name="stream"></param>
		/// <param name="templateId"></param>
		/// <param name="target"></param>
		/// <param name="outcomes"></param>
		/// <returns></returns>
		async Task<int> ExecuteChangeQueryAsync(IStream stream, TemplateRefId templateId, string? target, List<JobStepOutcome> outcomes)
		{
			IList<IJob> jobs = await _jobService.FindJobsAsync(streamId: stream.Id, templates: new[] { templateId }, target: target, state: new[] { JobStepState.Completed }, outcome: outcomes.ToArray(), count: 1, excludeUserJobs: true);
			if (jobs.Count == 0)
			{
				_logger.LogInformation("Unable to find successful build of {TemplateId} target {Target}. Using latest change instead", templateId, target);
				return await _perforce.GetLatestChangeAsync(stream.ClusterName, stream.Name, null);
			}
			else
			{
				_logger.LogInformation("Last successful build of {TemplateId} target {Target} was job {JobId} at change {Change}", templateId, target, jobs[0].Id, jobs[0].Change);
				return jobs[0].Change;
			}
		}

		/// <summary>
		/// Deletes a specific job.
		/// </summary>
		/// <param name="jobId">Id of the job to delete</param>
		/// <returns>Async task</returns>
		[HttpDelete]
		[Route("/api/v1/jobs/{jobId}")]
		public async Task<ActionResult> DeleteJobAsync(JobId jobId)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}
			if (!await _jobService.AuthorizeAsync(job, AclAction.DeleteJob, User, null))
			{
				return Forbid(AclAction.DeleteJob, jobId);
			}
			if (!await _jobService.DeleteJobAsync(job))
			{
				return NotFound(jobId);
			}
			return Ok();
		}

		/// <summary>
		/// Updates a specific job.
		/// </summary>
		/// <param name="jobId">Id of the job to find</param>
		/// <param name="request">Settings to update in the job</param>
		/// <returns>Async task</returns>
		[HttpPut]
		[Route("/api/v1/jobs/{jobId}")]
		public async Task<ActionResult> UpdateJobAsync(JobId jobId, [FromBody] UpdateJobRequest request)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}

			StreamPermissionsCache permissionsCache = new StreamPermissionsCache();
			if (!await _jobService.AuthorizeAsync(job, AclAction.UpdateJob, User, permissionsCache))
			{
				return Forbid(AclAction.UpdateJob, jobId);
			}
			if (request.Acl != null && !await _jobService.AuthorizeAsync(job, AclAction.ChangePermissions, User, permissionsCache))
			{
				return Forbid(AclAction.ChangePermissions, jobId);
			}

			// Convert legacy behavior of clearing out the argument to setting the aborted flag
			if (request.Arguments != null && request.Arguments.Count == 0)
			{
				request.Aborted = true;
				request.Arguments = null;
			}

			UserId? abortedByUserId = null;
			if (request.Aborted ?? false)
			{
				abortedByUserId = User.GetUserId();
			}

			IJob? newJob = await _jobService.UpdateJobAsync(job, name: request.Name, priority: request.Priority, autoSubmit: request.AutoSubmit, abortedByUserId: abortedByUserId, arguments: request.Arguments);
			if (newJob == null)
			{
				return NotFound(jobId);
			}
			return Ok();
		}

		/// <summary>
		/// Updates notifications for a specific job.
		/// </summary>
		/// <param name="jobId">Id of the job to find</param>
		/// <param name="request">The notification request</param>
		/// <returns>Information about the requested job</returns>
		[HttpPut]
		[Route("/api/v1/jobs/{jobId}/notifications")]
		public async Task<ActionResult> UpdateNotificationsAsync(JobId jobId, [FromBody] UpdateNotificationsRequest request)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}
			if (!await _jobService.AuthorizeAsync(job, AclAction.CreateSubscription, User, null))
			{
				return Forbid(AclAction.CreateSubscription, jobId);
			}

			ObjectId triggerId = job.NotificationTriggerId ?? ObjectId.GenerateNewId();

			job = await _jobService.UpdateJobAsync(job, null, null, null, null, triggerId, null, null);
			if (job == null)
			{
				return NotFound(jobId);
			}

			await _notificationService.UpdateSubscriptionsAsync(triggerId, User, request.Email, request.Slack);
			return Ok();
		}

		/// <summary>
		/// Gets information about a specific job.
		/// </summary>
		/// <param name="jobId">Id of the job to find</param>
		/// <returns>Information about the requested job</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/notifications")]
		public async Task<ActionResult<GetNotificationResponse>> GetNotificationsAsync(JobId jobId)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}
			if (!await _jobService.AuthorizeAsync(job, AclAction.CreateSubscription, User, null))
			{
				return Forbid(AclAction.CreateSubscription, jobId);
			}

			INotificationSubscription? subscription;
			if (job.NotificationTriggerId == null)
			{
				subscription = null;
			}
			else
			{
				subscription = await _notificationService.GetSubscriptionsAsync(job.NotificationTriggerId.Value, User);
			}
			return new GetNotificationResponse(subscription);
		}

		/// <summary>
		/// Gets information about a specific job.
		/// </summary>
		/// <param name="jobId">Id of the job to find</param>
		/// <param name="modifiedAfter">If specified, returns an empty response unless the job's update time is equal to or less than the given value</param>
		/// <param name="filter">Filter for the fields to return</param>
		/// <returns>Information about the requested job</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}")]
		[ProducesResponseType(typeof(GetJobResponse), 200)]
		public async Task<ActionResult<object>> GetJobAsync(JobId jobId, [FromQuery] DateTimeOffset? modifiedAfter = null, [FromQuery] PropertyFilter? filter = null)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}

			StreamPermissionsCache cache = new StreamPermissionsCache();
			if (!await _jobService.AuthorizeAsync(job, AclAction.ViewJob, User, cache))
			{
				return Forbid(AclAction.ViewJob, jobId);
			}
			if (modifiedAfter != null && job.UpdateTimeUtc <= modifiedAfter.Value)
			{
				return new Dictionary<string, object>();
			}

			IGraph graph = await _jobService.GetGraphAsync(job);
			bool bIncludeAcl = await _jobService.AuthorizeAsync(job, AclAction.ViewPermissions, User, cache);
			bool bIncludeCosts = await _jobService.AuthorizeAsync(job, AclAction.ViewCosts, User, cache);
			return await CreateJobResponseAsync(job, graph, bIncludeAcl, bIncludeCosts, filter);
		}

		/// <summary>
		/// Creates a response containing information about this object
		/// </summary>
		/// <param name="job">The job document</param>
		/// <param name="graph">The graph for this job</param>
		/// <param name="bIncludeAcl">Whether to include the ACL in the response</param>
		/// <param name="bIncludeCosts">Whether to include costs in the response</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>Object containing the requested properties</returns>
		async Task<object> CreateJobResponseAsync(IJob job, IGraph graph, bool bIncludeAcl, bool bIncludeCosts, PropertyFilter? filter)
		{
			if (filter == null)
			{
				return await CreateJobResponseAsync(job, graph, true, true, bIncludeAcl, bIncludeCosts);
			}
			else
			{
				return filter.ApplyTo(await CreateJobResponseAsync(job, graph, filter.Includes(nameof(GetJobResponse.Batches)), filter.Includes(nameof(GetJobResponse.Labels)) || filter.Includes(nameof(GetJobResponse.DefaultLabel)), bIncludeAcl, bIncludeCosts));
			}
		}

		/// <summary>
		/// Creates a response containing information about this object
		/// </summary>
		/// <param name="job">The job document</param>
		/// <param name="graph">The graph definition</param>
		/// <param name="bIncludeBatches">Whether to include the job batches in the response</param>
		/// <param name="bIncludeLabels">Whether to include the job aggregates in the response</param>
		/// <param name="bIncludeAcl">Whether to include the ACL in the response</param>
		/// <param name="bIncludeCosts">Whether to include costs of running particular agents</param>
		/// <returns>The response object</returns>
		async ValueTask<GetJobResponse> CreateJobResponseAsync(IJob job, IGraph graph, bool bIncludeBatches, bool bIncludeLabels, bool bIncludeAcl, bool bIncludeCosts)
		{
			GetThinUserInfoResponse? startedByUserInfo = null;
			if (job.StartedByUserId != null)
			{
				startedByUserInfo = new GetThinUserInfoResponse(await _userCollection.GetCachedUserAsync(job.StartedByUserId.Value));
			}

			GetThinUserInfoResponse? abortedByUserInfo = null;
			if (job.AbortedByUserId != null)
			{
				abortedByUserInfo = new GetThinUserInfoResponse(await _userCollection.GetCachedUserAsync(job.AbortedByUserId.Value));
			}

			GetAclResponse? aclResponse = null;
			if (bIncludeAcl && job.Acl != null)
			{
				aclResponse = new GetAclResponse(job.Acl);
			}

			GetJobResponse response = new GetJobResponse(job, startedByUserInfo, abortedByUserInfo, aclResponse);
			if (bIncludeBatches || bIncludeLabels)
			{
				if (bIncludeBatches)
				{
					response.Batches = new List<GetBatchResponse>();
					foreach (IJobStepBatch batch in job.Batches)
					{
						response.Batches.Add(await CreateBatchResponseAsync(batch, bIncludeCosts));
					}
				}
				if (bIncludeLabels)
				{
					response.Labels = new List<GetLabelStateResponse>();
					response.DefaultLabel = job.GetLabelStateResponses(graph, response.Labels);
				}
			}
			return response;
		}

		/// <summary>
		/// Get the response object for a batch
		/// </summary>
		/// <param name="batch"></param>
		/// <param name="bIncludeCosts"></param>
		/// <returns></returns>
		async ValueTask<GetBatchResponse> CreateBatchResponseAsync(IJobStepBatch batch, bool bIncludeCosts)
		{
			List<GetStepResponse> steps = new List<GetStepResponse>();
			foreach (IJobStep step in batch.Steps)
			{
				steps.Add(await CreateStepResponseAsync(step));
			}

			double? agentRate = null;
			if (batch.AgentId != null && bIncludeCosts)
			{
				agentRate = await _agentService.GetRateAsync(batch.AgentId.Value);
			}

			return new GetBatchResponse(batch, steps, agentRate);
		}

		/// <summary>
		/// Get the response object for a step
		/// </summary>
		/// <param name="step"></param>
		/// <returns></returns>
		async ValueTask<GetStepResponse> CreateStepResponseAsync(IJobStep step)
		{
			GetThinUserInfoResponse? abortedByUserInfo = null;
			if (step.AbortedByUserId != null)
			{
				abortedByUserInfo = new GetThinUserInfoResponse(await _userCollection.GetCachedUserAsync(step.AbortedByUserId.Value));
			}

			GetThinUserInfoResponse? retriedByUserInfo = null;
			if (step.RetriedByUserId != null)
			{
				retriedByUserInfo = new GetThinUserInfoResponse(await _userCollection.GetCachedUserAsync(step.RetriedByUserId.Value));
			}

			return new GetStepResponse(step, abortedByUserInfo, retriedByUserInfo);
		}

		/// <summary>
		/// Gets information about the graph for a specific job.
		/// </summary>
		/// <param name="jobId">Id of the job to find</param>
		/// <param name="filter">Filter for the fields to return</param>
		/// <returns>Information about the requested job</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/graph")]
		[ProducesResponseType(typeof(GetGraphResponse), 200)]
		public async Task<ActionResult<object>> GetJobGraphAsync(JobId jobId, [FromQuery] PropertyFilter? filter = null)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}
			if (!await _jobService.AuthorizeAsync(job, AclAction.ViewJob, User, null))
			{
				return Forbid(AclAction.ViewJob, job.Id);
			}

			IGraph graph = await _jobService.GetGraphAsync(job);
			return PropertyFilter.Apply(new GetGraphResponse(graph), filter);
		}

		/// <summary>
		/// Gets timing information about the graph for a specific job.
		/// </summary>
		/// <param name="jobId">Id of the job to find</param>
		/// <param name="filter">Filter for the fields to return</param>
		/// <returns>Information about the requested job</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/timing")]
		[ProducesResponseType(typeof(GetJobTimingResponse), 200)]
		public async Task<ActionResult<object>> GetJobTimingAsync(JobId jobId, [FromQuery] PropertyFilter? filter = null)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}
			if (!await _jobService.AuthorizeAsync(job, AclAction.ViewJob, User, null))
			{
				return Forbid(AclAction.ViewJob, jobId);
			}

			IJobTiming jobTiming = await _jobService.GetJobTimingAsync(job);
			IGraph graph = await _jobService.GetGraphAsync(job);
			return PropertyFilter.Apply(await CreateJobTimingResponse(job, graph, jobTiming), filter);
		}

		private async Task<GetJobTimingResponse> CreateJobTimingResponse(IJob job, IGraph graph, IJobTiming jobTiming, bool includeJobResponse = false)
		{
			Dictionary<INode, TimingInfo> nodeToTimingInfo = job.GetTimingInfo(graph, jobTiming);

			Dictionary<string, GetStepTimingInfoResponse> steps = new Dictionary<string, GetStepTimingInfoResponse>();
			foreach (IJobStepBatch batch in job.Batches)
			{
				foreach (IJobStep step in batch.Steps)
				{
					INode node = graph.Groups[batch.GroupIdx].Nodes[step.NodeIdx];
					steps[step.Id.ToString()] = new GetStepTimingInfoResponse(node.Name, nodeToTimingInfo[node]);
				}
			}

			List<GetLabelTimingInfoResponse> labels = new List<GetLabelTimingInfoResponse>();
			foreach (ILabel label in graph.Labels)
			{
				TimingInfo timingInfo = TimingInfo.Max(label.GetDependencies(graph.Groups).Select(x => nodeToTimingInfo[x]));
				labels.Add(new GetLabelTimingInfoResponse(label, timingInfo));
			}

			GetJobResponse? jobResponse = null;
			if (includeJobResponse)
			{
				jobResponse = await CreateJobResponseAsync(job, graph, true, true, false, true);
			}

			return new GetJobTimingResponse(jobResponse, steps, labels);
		}
		
		/// <summary>
		/// Find timing information about the graph for multiple jobs
		/// </summary>
		/// <param name="streamId">The stream to search in</param>
		/// <param name="templates">List of templates to find</param>
		/// <param name="filter">Filter for the fields to return</param>
		/// <param name="count">Number of results to return</param>
		/// <returns>Job timings for each job ID</returns>
		[HttpGet]
		[Route("/api/v1/jobs/timing")]
		[ProducesResponseType(typeof(FindJobTimingsResponse), 200)]
		public async Task<ActionResult<object>> FindJobTimingsAsync(
			[FromQuery] string? streamId = null,
			[FromQuery(Name = "template")] string[]? templates = null,
			[FromQuery] PropertyFilter? filter = null,
			[FromQuery] int count = 100)
		{
			if (streamId == null)
			{
				return BadRequest("Missing/invalid query parameter streamId");
			}

			TemplateRefId[] templateRefIds = templates switch
			{
				{ Length: > 0 } => templates.Select(x => new TemplateRefId(x)).ToArray(),
				_ => Array.Empty<TemplateRefId>()
			};

			List<IJob> jobs = await _jobService.FindJobsByStreamWithTemplatesAsync(new StreamId(streamId), templateRefIds, count: count, consistentRead: false);

			Dictionary<string, GetJobTimingResponse> jobTimings = await jobs.ToAsyncEnumerable()
				.WhereAwait(async job => await _jobService.AuthorizeAsync(job, AclAction.ViewJob, User, null))
				.ToDictionaryAwaitAsync(x => ValueTask.FromResult(x.Id.ToString()), async job =>
				{
					IJobTiming jobTiming = await _jobService.GetJobTimingAsync(job);
					IGraph graph = await _jobService.GetGraphAsync(job);
					return await CreateJobTimingResponse(job, graph, jobTiming, true);
				});
			
			return PropertyFilter.Apply(new FindJobTimingsResponse(jobTimings), filter);
		}

		/// <summary>
		/// Gets information about the template for a specific job.
		/// </summary>
		/// <param name="jobId">Id of the job to find</param>
		/// <param name="filter">Filter for the fields to return</param>
		/// <returns>Information about the requested job</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/template")]
		[ProducesResponseType(typeof(GetTemplateResponse), 200)]
		public async Task<ActionResult<object>> GetJobTemplateAsync(JobId jobId, [FromQuery] PropertyFilter? filter = null)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null || job.TemplateHash == null)
			{
				return NotFound(jobId);
			}
			if (!await _jobService.AuthorizeAsync(job, AclAction.ViewJob, User, null))
			{
				return Forbid(AclAction.ViewJob, jobId);
			}

			ITemplate? template = await _templateCollection.GetAsync(job.TemplateHash);
			if(template == null)
			{
				return NotFound(job.StreamId, job.TemplateId);
			}

			return new GetTemplateResponse(template).ApplyFilter(filter);
		}

		/// <summary>
		/// Find jobs matching a criteria
		/// </summary>
		/// <param name="ids">The job ids to return</param>
		/// <param name="name">Name of the job to find</param>
		/// <param name="templates">List of templates to find</param>
		/// <param name="streamId">The stream to search for</param>
		/// <param name="minChange">The minimum changelist number</param>
		/// <param name="maxChange">The maximum changelist number</param>
		/// <param name="includePreflight">Whether to include preflight jobs</param>
		/// <param name="preflightOnly">Whether to only include preflight jobs</param>
		/// <param name="preflightChange">The preflighted changelist</param>
		/// <param name="startedByUserId">User id for which to include jobs</param>
		/// <param name="preflightStartedByUserId">User id for which to include preflight jobs</param>
		/// <param name="minCreateTime">Minimum creation time</param>
		/// <param name="maxCreateTime">Maximum creation time</param>
		/// <param name="modifiedBefore">If specified, only jobs updated before the give time will be returned</param>
		/// <param name="modifiedAfter">If specified, only jobs updated after the give time will be returned</param>
		/// <param name="target">Target to filter the returned jobs by</param>
		/// <param name="state">Filter state of the returned jobs</param>
		/// <param name="outcome">Filter outcome of the returned jobs</param>
		/// <param name="filter">Filter for properties to return</param>
		/// <param name="index">Index of the first result to be returned</param>
		/// <param name="count">Number of results to return</param>
		/// <returns>List of jobs</returns>
		[HttpGet]
		[Route("/api/v1/jobs")]
		[ProducesResponseType(typeof(List<GetJobResponse>), 200)]
		public async Task<ActionResult<List<object>>> FindJobsAsync(
			[FromQuery(Name = "Id")] string[]? ids = null,
			[FromQuery] string? streamId = null,
			[FromQuery] string? name = null,
			[FromQuery(Name = "template")] string[]? templates = null,
			[FromQuery] int? minChange = null,
			[FromQuery] int? maxChange = null,
			[FromQuery] bool includePreflight = true,
			[FromQuery] bool? preflightOnly = null,
			[FromQuery] int? preflightChange = null,
			[FromQuery] string? preflightStartedByUserId = null,
			[FromQuery] string? startedByUserId = null,
			[FromQuery] DateTimeOffset? minCreateTime = null,
			[FromQuery] DateTimeOffset? maxCreateTime = null,
			[FromQuery] DateTimeOffset? modifiedBefore = null,
			[FromQuery] DateTimeOffset? modifiedAfter = null,
			[FromQuery] string? target = null,
			[FromQuery] JobStepState[]? state = null,
			[FromQuery] JobStepOutcome[]? outcome = null, 
			[FromQuery] PropertyFilter? filter = null,
			[FromQuery] int index = 0,
			[FromQuery] int count = 100)
		{
			JobId[]? jobIdValues = (ids == null) ? (JobId[]?)null : Array.ConvertAll(ids, x => new JobId(x));
			StreamId? streamIdValue = (streamId == null)? (StreamId?)null : new StreamId(streamId);
			
			TemplateRefId[]? templateRefIds = (templates != null && templates.Length > 0) ? templates.Select(x => new TemplateRefId(x)).ToArray() : null;

			if (includePreflight == false)
			{
				preflightChange = 0;
			}

			UserId? preflightStartedByUserIdValue = null;

			if (preflightStartedByUserId != null)
			{
				preflightStartedByUserIdValue = new UserId(preflightStartedByUserId);
			}

			UserId? startedByUserIdValue = null;

			if (startedByUserId != null)
			{
				startedByUserIdValue = new UserId(startedByUserId);
			}

			List<IJob> jobs;
			using (IScope _ = GlobalTracer.Instance.BuildSpan("FindJobs").StartActive())
			{
				jobs = await _jobService.FindJobsAsync(jobIdValues, streamIdValue, name, templateRefIds, minChange,
					maxChange, preflightChange, preflightOnly, preflightStartedByUserIdValue, startedByUserIdValue, minCreateTime?.UtcDateTime, maxCreateTime?.UtcDateTime, target, state, outcome,
					modifiedBefore, modifiedAfter, index, count, false);
			}

			StreamPermissionsCache permissionsCache = new StreamPermissionsCache();

			List<object> responses = new List<object>();
			foreach (IJob job in jobs)
			{
				using IScope jobScope = GlobalTracer.Instance.BuildSpan("JobIteration").StartActive();
				jobScope.Span.SetTag("jobId", job.Id.ToString());

				if (job.GraphHash == null)
				{
					_logger.LogWarning("Job {JobId} has no GraphHash", job.Id);
					continue;
				}

				bool viewJobAuthorized;
				using (IScope _ = GlobalTracer.Instance.BuildSpan("AuthorizeViewJob").StartActive())
				{
					viewJobAuthorized = await _jobService.AuthorizeAsync(job, AclAction.ViewJob, User, permissionsCache);
				}
				
				if (viewJobAuthorized)
				{
					IGraph graph;
					using (IScope _ = GlobalTracer.Instance.BuildSpan("GetGraph").StartActive())
					{
						graph = await _jobService.GetGraphAsync(job);
					}

					bool bIncludeAcl;
					using (IScope _ = GlobalTracer.Instance.BuildSpan("AuthorizeViewPermissions").StartActive())
					{
						bIncludeAcl = await _jobService.AuthorizeAsync(job, AclAction.ViewPermissions, User, permissionsCache);
					}

					bool bIncludeCosts;
					using (IScope _ = GlobalTracer.Instance.BuildSpan("AuthorizeViewCosts").StartActive())
					{
						bIncludeCosts = await _jobService.AuthorizeAsync(job, AclAction.ViewCosts, User, permissionsCache);
					}

					using (IScope _ = GlobalTracer.Instance.BuildSpan("CreateResponse").StartActive())
					{
						responses.Add(await CreateJobResponseAsync(job, graph, bIncludeAcl, bIncludeCosts, filter));
					}
				}
			}
			return responses;
		}

		/// <summary>
		/// Find jobs for a stream with given templates, sorted by creation date
		/// </summary>
		/// <param name="streamId">The stream to search for</param>
		/// <param name="templates">List of templates to find</param>
		/// <param name="preflightStartedByUserId">User id for which to include preflight jobs</param>
		/// <param name="maxCreateTime">Maximum creation time</param>
		/// <param name="modifiedAfter">If specified, only jobs updated after the given time will be returned</param>
		/// <param name="filter">Filter for properties to return</param>
		/// <param name="index">Index of the first result to be returned</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="consistentRead">If a read to the primary database is required, for read consistency. Usually not required.</param>
		/// <returns>List of jobs</returns>
		[HttpGet]
		[Route("/api/v1/jobs/streams/{streamId}")]
		[ProducesResponseType(typeof(List<GetJobResponse>), 200)]
		public async Task<ActionResult<List<object>>> FindJobsByStreamWithTemplatesAsync(
			string streamId,
			[FromQuery(Name = "template")] string[] templates,
			[FromQuery] string? preflightStartedByUserId = null,
			[FromQuery] DateTimeOffset? maxCreateTime = null,
			[FromQuery] DateTimeOffset? modifiedAfter = null,
			[FromQuery] PropertyFilter? filter = null,
			[FromQuery] int index = 0,
			[FromQuery] int count = 100,
			[FromQuery] bool consistentRead = false)
		{
			StreamId streamIdValue = new StreamId(streamId);
			TemplateRefId[] templateRefIds = templates.Select(x => new TemplateRefId(x)).ToArray();
			UserId? preflightStartedByUserIdValue = preflightStartedByUserId != null ? new UserId(preflightStartedByUserId) : null;
			count = Math.Min(1000, count);

			List<IJob> jobs = await _jobService.FindJobsByStreamWithTemplatesAsync(streamIdValue, templateRefIds, preflightStartedByUserIdValue, maxCreateTime, modifiedAfter, index, count, consistentRead);
			return await CreateAuthorizedJobResponses(jobs, filter);
		}

		private async Task<List<object>> CreateAuthorizedJobResponses(List<IJob> jobs, PropertyFilter? filter = null)
		{
			StreamPermissionsCache permissionsCache = new ();
			List<object> responses = new ();
			foreach (IJob job in jobs)
			{
				using IScope jobScope = GlobalTracer.Instance.BuildSpan("JobIteration").StartActive();
				jobScope.Span.SetTag("jobId", job.Id.ToString());
				
				bool viewJobAuthorized;
				using (IScope _ = GlobalTracer.Instance.BuildSpan("AuthorizeViewJob").StartActive())
				{
					viewJobAuthorized = await _jobService.AuthorizeAsync(job, AclAction.ViewJob, User, permissionsCache);
				}
				
				if (viewJobAuthorized)
				{
					IGraph graph;
					using (IScope _ = GlobalTracer.Instance.BuildSpan("GetGraph").StartActive())
					{
						graph = await _jobService.GetGraphAsync(job);
					}

					bool bIncludeAcl;
					using (IScope _ = GlobalTracer.Instance.BuildSpan("AuthorizeViewPermissions").StartActive())
					{
						bIncludeAcl = await _jobService.AuthorizeAsync(job, AclAction.ViewPermissions, User, permissionsCache);
					}

					bool bIncludeCosts;
					using (IScope _ = GlobalTracer.Instance.BuildSpan("AuthorizeViewCosts").StartActive())
					{
						bIncludeCosts = await _jobService.AuthorizeAsync(job, AclAction.ViewCosts, User, permissionsCache);
					}

					using (IScope _ = GlobalTracer.Instance.BuildSpan("CreateResponse").StartActive())
					{
						responses.Add(await CreateJobResponseAsync(job, graph, bIncludeAcl, bIncludeCosts, filter));
					}
				}
			}

			
			return responses;
		}

		/// <summary>
		/// Adds an array of nodes to be executed for a job
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="requests">Properties of the new nodes</param>
		/// <returns>Id of the new job</returns>
		[HttpPost]
		[Route("/api/v1/jobs/{jobId}/groups")]
		public async Task<ActionResult> CreateGroupsAsync(JobId jobId, [FromBody] List<NewGroup> requests)
		{
			for (; ; )
			{
				IJob? job = await _jobService.GetJobAsync(jobId);
				if (job == null)
				{
					return NotFound(jobId);
				}
				if (!await _jobService.AuthorizeAsync(job, AclAction.ExecuteJob, User, null))
				{
					return Forbid(AclAction.ExecuteJob, jobId);
				}

				IGraph graph = await _jobService.GetGraphAsync(job);
				graph = await _graphs.AppendAsync(graph, requests, null, null);

				IJob? newJob = await _jobService.TryUpdateGraphAsync(job, graph);
				if (newJob != null)
				{
					return Ok();
				}
			}
		}

		/// <summary>
		/// Gets the nodes to be executed for a job
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>List of nodes to be executed</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/groups")]
		[ProducesResponseType(typeof(List<GetGroupResponse>), 200)]
		public async Task<ActionResult<List<object>>> GetGroupsAsync(JobId jobId, [FromQuery] PropertyFilter? filter = null)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}
			if (!await _jobService.AuthorizeAsync(job, AclAction.ViewJob, User, null))
			{
				return Forbid(AclAction.ViewJob, jobId);
			}

			IGraph graph = await _jobService.GetGraphAsync(job);
			return graph.Groups.ConvertAll(x => new GetGroupResponse(x, graph.Groups).ApplyFilter(filter));
		}

		/// <summary>
		/// Gets the nodes in a group to be executed for a job
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="groupIdx">The group index</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>List of nodes to be executed</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/groups/{groupIdx}")]
		[ProducesResponseType(typeof(GetGroupResponse), 200)]
		public async Task<ActionResult<object>> GetGroupAsync(JobId jobId, int groupIdx, [FromQuery] PropertyFilter? filter = null)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}
			if (!await _jobService.AuthorizeAsync(job, AclAction.ViewJob, User, null))
			{
				return Forbid(AclAction.ViewJob, jobId);
			}

			IGraph graph = await _jobService.GetGraphAsync(job);
			if (groupIdx < 0 || groupIdx >= graph.Groups.Count)
			{
				return NotFound(jobId, groupIdx);
			}

			return new GetGroupResponse(graph.Groups[groupIdx], graph.Groups).ApplyFilter(filter);
		}

		/// <summary>
		/// Gets the nodes for a particular group
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="groupIdx">Index of the group containing the node to update</param>
		/// <param name="filter">Filter for the properties to return</param>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/groups/{groupIdx}/nodes")]
		[ProducesResponseType(typeof(List<GetNodeResponse>), 200)]
		public async Task<ActionResult<List<object>>> GetNodesAsync(JobId jobId, int groupIdx, [FromQuery] PropertyFilter? filter = null)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}
			if (!await _jobService.AuthorizeAsync(job, AclAction.ViewJob, User, null))
			{
				return Forbid(AclAction.ViewJob, jobId);
			}

			IGraph graph = await _jobService.GetGraphAsync(job);
			if (groupIdx < 0 || groupIdx >= graph.Groups.Count)
			{
				return NotFound(jobId, groupIdx);
			}

			return graph.Groups[groupIdx].Nodes.ConvertAll(x => new GetNodeResponse(x, graph.Groups).ApplyFilter(filter));
		}

		/// <summary>
		/// Gets a particular node definition
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="groupIdx">Index of the group containing the node to update</param>
		/// <param name="nodeIdx">Index of the node to update</param>
		/// <param name="filter">Filter for the properties to return</param>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/groups/{groupIdx}/nodes/{nodeIdx}")]
		[ProducesResponseType(typeof(GetNodeResponse), 200)]
		public async Task<ActionResult<object>> GetNodeAsync(JobId jobId, int groupIdx, int nodeIdx, [FromQuery] PropertyFilter? filter = null)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}
			if (!await _jobService.AuthorizeAsync(job, AclAction.ViewJob, User, null))
			{
				return Forbid(AclAction.ViewJob, jobId);
			}

			IGraph graph = await _jobService.GetGraphAsync(job);
			if (groupIdx < 0 || groupIdx >= graph.Groups.Count)
			{
				return NotFound(jobId, groupIdx);
			}
			if(nodeIdx < 0 || nodeIdx >= graph.Groups[groupIdx].Nodes.Count)
			{
				return NotFound(jobId, groupIdx, nodeIdx);
			}

			return new GetNodeResponse(graph.Groups[groupIdx].Nodes[nodeIdx], graph.Groups).ApplyFilter(filter);
		}

		/// <summary>
		/// Gets the steps currently scheduled to be executed for a job
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>List of nodes to be executed</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/batches")]
		[ProducesResponseType(typeof(List<GetBatchResponse>), 200)]
		public async Task<ActionResult<List<object>>> GetBatchesAsync(JobId jobId, [FromQuery] PropertyFilter? filter = null)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}

			StreamPermissionsCache cache = new StreamPermissionsCache();
			if (!await _jobService.AuthorizeAsync(job, AclAction.ViewJob, User, cache))
			{
				return Forbid(AclAction.ViewJob, jobId);
			}

			bool bIncludeCosts = await _jobService.AuthorizeAsync(job, AclAction.ViewCosts, User, cache);

			List<object> responses = new List<object>();
			foreach (IJobStepBatch batch in job.Batches)
			{
				GetBatchResponse response = await CreateBatchResponseAsync(batch, bIncludeCosts);
				responses.Add(response.ApplyFilter(filter));
			}
			return responses;
		}

		/// <summary>
		/// Updates the state of a jobstep
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="batchId">Unique id for the step</param>
		/// <param name="request">Updates to apply to the node</param>
		[HttpPut]
		[Route("/api/v1/jobs/{jobId}/batches/{batchId}")]
		public async Task<ActionResult> UpdateBatchAsync(JobId jobId, SubResourceId batchId, [FromBody] UpdateBatchRequest request)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}

			IJobStepBatch? batch = job.Batches.FirstOrDefault(x => x.Id == batchId);
			if (batch == null)
			{
				return NotFound(jobId, batchId);
			}
			if (batch.SessionId == null || !User.HasSessionClaim(batch.SessionId.Value))
			{
				return Forbid("Missing session claim for job {JobId} batch {BatchId}", jobId, batchId);
			}

			IJob? newJob = await _jobService.UpdateBatchAsync(job, batchId, request.LogId?.ToObjectId<ILogFile>(), request.State);
			if (newJob == null)
			{
				return NotFound(jobId);
			}
			return Ok();
		}

		/// <summary>
		/// Gets a particular step currently scheduled to be executed for a job
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="batchId">Unique id for the step</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>List of nodes to be executed</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/batches/{batchId}")]
		[ProducesResponseType(typeof(GetBatchResponse), 200)]
		public async Task<ActionResult<object>> GetBatchAsync(JobId jobId, SubResourceId batchId, [FromQuery] PropertyFilter? filter = null)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}

			StreamPermissionsCache cache = new StreamPermissionsCache();
			if (!await _jobService.AuthorizeAsync(job, AclAction.ViewJob, User, cache))
			{
				return Forbid(AclAction.ViewJob, jobId);
			}

			bool bIncludeCosts = await _jobService.AuthorizeAsync(job, AclAction.ViewCosts, User, cache);

			foreach (IJobStepBatch batch in job.Batches)
			{
				if (batch.Id == batchId)
				{
					GetBatchResponse response = await CreateBatchResponseAsync(batch, bIncludeCosts);
					return response.ApplyFilter(filter);
				}
			}

			return NotFound(jobId, batchId);
		}

		/// <summary>
		/// Gets the steps currently scheduled to be executed for a job
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="batchId">Unique id for the batch</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>List of nodes to be executed</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/batches/{batchId}/steps")]
		[ProducesResponseType(typeof(List<GetStepResponse>), 200)]
		public async Task<ActionResult<List<object>>> GetStepsAsync(JobId jobId, SubResourceId batchId, [FromQuery] PropertyFilter? filter = null)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}
			if (!await _jobService.AuthorizeAsync(job, AclAction.ViewJob, User, null))
			{
				return Forbid(AclAction.ViewJob, jobId);
			}

			foreach (IJobStepBatch batch in job.Batches)
			{
				if (batch.Id == batchId)
				{
					List<object> responses = new List<object>();
					foreach (IJobStep step in batch.Steps)
					{
						GetStepResponse response = await CreateStepResponseAsync(step);
						responses.Add(response.ApplyFilter(filter));
					}
					return responses;
				}
			}

			return NotFound(jobId, batchId);
		}

		/// <summary>
		/// Updates the state of a jobstep
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="batchId">Unique id for the batch</param>
		/// <param name="stepId">Unique id for the step</param>
		/// <param name="request">Updates to apply to the node</param>
		[HttpPut]
		[Route("/api/v1/jobs/{jobId}/batches/{batchId}/steps/{stepId}")]
		public async Task<ActionResult<UpdateStepResponse>> UpdateStepAsync(JobId jobId, SubResourceId batchId, SubResourceId stepId, [FromBody] UpdateStepRequest request)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}

			// Check permissions for updating this step. Only the agent executing the step can modify the state of it.
			if (request.State != JobStepState.Unspecified || request.Outcome != JobStepOutcome.Unspecified)
			{
				IJobStepBatch? batch = job.Batches.FirstOrDefault(x => x.Id == batchId);
				if (batch == null)
				{
					return NotFound(jobId, batchId);
				}
				if (!batch.SessionId.HasValue || !User.HasSessionClaim(batch.SessionId.Value))
				{
					return Forbid();
				}
			}
			if (request.Retry != null || request.Priority != null)
			{
				if (!await _jobService.AuthorizeAsync(job, AclAction.RetryJobStep, User, null))
				{
					return Forbid(AclAction.RetryJobStep, jobId);
				}
			}
			if (request.Properties != null)
			{
				if (!await _jobService.AuthorizeAsync(job, AclAction.UpdateJob, User, null))
				{
					return Forbid(AclAction.UpdateJob, jobId);
				}
			}

			UserId? retryByUser = (request.Retry.HasValue && request.Retry.Value) ? User.GetUserId() : null;
			UserId? abortByUser = (request.AbortRequested.HasValue && request.AbortRequested.Value) ? User.GetUserId() : null;

			try
			{
				IJob? newJob = await _jobService.UpdateStepAsync(job, batchId, stepId, request.State, request.Outcome, null, request.AbortRequested, abortByUser, request.LogId?.ToObjectId<ILogFile>(), null, retryByUser, request.Priority, null, request.Properties);
				if (newJob == null)
				{
					return NotFound(jobId);
				}

				UpdateStepResponse response = new UpdateStepResponse();
				if (request.Retry ?? false)
				{
					JobStepRefId? retriedStepId = FindRetriedStep(job, batchId, stepId);
					if (retriedStepId != null)
					{
						response.BatchId = retriedStepId.Value.BatchId.ToString();
						response.StepId = retriedStepId.Value.StepId.ToString();
					}
				}
				return response;
			}
			catch (RetryNotAllowedException ex)
			{
				return BadRequest(ex.Message);
			}
		}

		/// <summary>
		/// Find the first retried step after the given step
		/// </summary>
		/// <param name="job">The job being run</param>
		/// <param name="batchId">Batch id of the last step instance</param>
		/// <param name="stepId">Step id of the last instance</param>
		/// <returns>The retried step information</returns>
		static JobStepRefId? FindRetriedStep(IJob job, SubResourceId batchId, SubResourceId stepId)
		{
			NodeRef? lastNodeRef = null;
			foreach (IJobStepBatch batch in job.Batches)
			{
				if ((lastNodeRef == null && batch.Id == batchId) || (lastNodeRef != null && batch.GroupIdx == lastNodeRef.GroupIdx))
				{
					foreach (IJobStep step in batch.Steps)
					{
						if (lastNodeRef == null && step.Id == stepId)
						{
							lastNodeRef = new NodeRef(batch.GroupIdx, step.NodeIdx);
						}
						else if (lastNodeRef != null && step.NodeIdx == lastNodeRef.NodeIdx)
						{
							return new JobStepRefId(job.Id, batch.Id, step.Id);
						}
					}
				}
			}
			return null;
		}

		/// <summary>
		/// Gets a particular step currently scheduled to be executed for a job
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="batchId">Unique id for the batch</param>
		/// <param name="stepId">Unique id for the step</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>List of nodes to be executed</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/batches/{batchId}/steps/{stepId}")]
		[ProducesResponseType(typeof(GetStepResponse), 200)]
		public async Task<ActionResult<object>> GetStepAsync(JobId jobId, SubResourceId batchId, SubResourceId stepId, [FromQuery] PropertyFilter? filter = null)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}
			if (!await _jobService.AuthorizeAsync(job, AclAction.ViewJob, User, null))
			{
				return Forbid(AclAction.ViewJob, jobId);
			}

			foreach (IJobStepBatch batch in job.Batches)
			{
				if (batch.Id == batchId)
				{
					foreach (IJobStep step in batch.Steps)
					{
						if (step.Id == stepId)
						{
							GetStepResponse response = await CreateStepResponseAsync(step);
							return response.ApplyFilter(filter);
						}
					}
					return NotFound(jobId, batchId, stepId);
				}
			}

			return NotFound(jobId, batchId);
		}

		/// <summary>
		/// Updates notifications for a specific job.
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="batchId">Unique id for the batch</param>
		/// <param name="stepId">Unique id for the step</param>
		/// <param name="request">The notification request</param>
		/// <returns>Information about the requested job</returns>
		[HttpPut]
		[Route("/api/v1/jobs/{jobId}/batches/{batchId}/steps/{stepId}/notifications")]
		public async Task<ActionResult> UpdateStepNotificationsAsync(JobId jobId, SubResourceId batchId, SubResourceId stepId, [FromBody] UpdateNotificationsRequest request)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}
			if (!await _jobService.AuthorizeAsync(job, AclAction.CreateSubscription, User, null))
			{
				return Forbid(AclAction.CreateSubscription, jobId);
			}

			if (!job.TryGetBatch(batchId, out IJobStepBatch? batch))
			{
				return NotFound(jobId, batchId);
			}

			if (!batch.TryGetStep(stepId, out IJobStep? step))
			{
				return NotFound(jobId, batchId, stepId);
			}

			ObjectId? triggerId = step.NotificationTriggerId;
			if (triggerId == null)
			{
				triggerId = ObjectId.GenerateNewId();
				if (await _jobService.UpdateStepAsync(job, batchId, stepId, JobStepState.Unspecified, JobStepOutcome.Unspecified, newNotificationTriggerId: triggerId) == null)
				{
					return NotFound(jobId, batchId, stepId);
				}
			}

			await _notificationService.UpdateSubscriptionsAsync(triggerId.Value, User, request.Email, request.Slack);
			return Ok();
		}

		/// <summary>
		/// Gets information about a specific job.
		/// </summary>
		/// <param name="jobId">Id of the job to find</param>
		/// <param name="batchId">Unique id for the batch</param>
		/// <param name="stepId">Unique id for the step</param>
		/// <returns>Information about the requested job</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/batches/{batchId}/steps/{stepId}/notifications")]
		public async Task<ActionResult<GetNotificationResponse>> GetStepNotificationsAsync(JobId jobId, SubResourceId batchId, SubResourceId stepId)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}

			IJobStep? step;
			if (!job.TryGetBatch(batchId, out IJobStepBatch? batch))
			{
				return NotFound(jobId, batchId);
			}
			if (!batch.TryGetStep(stepId, out step))
			{
				return NotFound(jobId, batchId, stepId);
			}
			if (!await _jobService.AuthorizeAsync(job, AclAction.CreateSubscription, User, null))
			{
				return Forbid(AclAction.CreateSubscription, jobId);
			}

			INotificationSubscription? subscription;
			if (step.NotificationTriggerId == null)
			{
				subscription = null;
			}
			else
			{
				subscription = await _notificationService.GetSubscriptionsAsync(step.NotificationTriggerId.Value, User);
			}
			return new GetNotificationResponse(subscription);
		}

		/// <summary>
		/// Gets a particular step currently scheduled to be executed for a job
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="batchId">Unique id for the batch</param>
		/// <param name="stepId">Unique id for the step</param>
		/// <param name="name"></param>
		/// <returns>List of nodes to be executed</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/batches/{batchId}/steps/{stepId}/artifacts/{*name}")]
		public async Task<ActionResult> GetArtifactAsync(JobId jobId, SubResourceId batchId, SubResourceId stepId, string name)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}
			if (!await _jobService.AuthorizeAsync(job, AclAction.ViewJob, User, null))
			{
				return Forbid(AclAction.ViewJob, jobId);
			}
			if (!job.TryGetBatch(batchId, out IJobStepBatch? batch))
			{
				return NotFound(jobId, batchId);
			}
			if (!batch.TryGetStep(stepId, out _))
			{
				return NotFound(jobId, batchId, stepId);
			}

			List<IArtifact> artifacts = await _artifactCollection.GetArtifactsAsync(jobId, stepId, name);
			if (artifacts.Count == 0)
			{
				return NotFound();
			}

			IArtifact artifact = artifacts[0];
			return new FileStreamResult(await _artifactCollection.OpenArtifactReadStreamAsync(artifact), artifact.MimeType);
		}

		/// <summary>
		/// Gets a particular step currently scheduled to be executed for a job
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="batchId">Unique id for the batch</param>
		/// <param name="stepId">Unique id for the step</param>
		/// <returns>List of nodes to be executed</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/batches/{batchId}/steps/{stepId}/trace")]
		public async Task<ActionResult> GetStepTraceAsync(JobId jobId, SubResourceId batchId, SubResourceId stepId)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}
			if (!await _jobService.AuthorizeAsync(job, AclAction.ViewJob, User, null))
			{
				return Forbid(AclAction.ViewJob, jobId);
			}
			if (!job.TryGetBatch(batchId, out IJobStepBatch? batch))
			{
				return NotFound(jobId, batchId);
			}
			if (!batch.TryGetStep(stepId, out _))
			{
				return NotFound(jobId, batchId, stepId);
			}

			List<IArtifact> artifacts = await _artifactCollection.GetArtifactsAsync(jobId, stepId, null);
			foreach (IArtifact artifact in artifacts)
			{
				if (artifact.Name.Equals("trace.json", StringComparison.OrdinalIgnoreCase))
				{
					return new FileStreamResult(await _artifactCollection.OpenArtifactReadStreamAsync(artifact), "text/json");
				}
			}
			return NotFound();
		}

		/// <summary>
		/// Updates notifications for a specific label.
		/// </summary>
		/// <param name="jobId">Unique id for the job</param>
		/// <param name="labelIndex">Index for the label</param>
		/// <param name="request">The notification request</param>
		[HttpPut]
		[Route("/api/v1/jobs/{jobId}/labels/{labelIndex}/notifications")]
		public async Task<ActionResult> UpdateLabelNotificationsAsync(JobId jobId, int labelIndex, [FromBody] UpdateNotificationsRequest request)
		{
			ObjectId triggerId;
			for (; ; )
			{
				IJob? job = await _jobService.GetJobAsync(jobId);
				if (job == null)
				{
					return NotFound(jobId);
				}
				if (!await _jobService.AuthorizeAsync(job, AclAction.CreateSubscription, User, null))
				{
					return Forbid(AclAction.CreateSubscription, jobId);
				}

				ObjectId newTriggerId;
				if (job.LabelIdxToTriggerId.TryGetValue(labelIndex, out newTriggerId))
				{
					triggerId = newTriggerId;
					break;
				}

				newTriggerId = ObjectId.GenerateNewId();

				IJob? newJob = await _jobService.UpdateJobAsync(job, labelIdxToTriggerId: new KeyValuePair<int, ObjectId>(labelIndex, newTriggerId));
				if (newJob != null)
				{
					triggerId = newTriggerId;
					break;
				}
			}

			await _notificationService.UpdateSubscriptionsAsync(triggerId, User, request.Email, request.Slack);
			return Ok();
		}

		/// <summary>
		/// Gets notification info about a specific label in a job.
		/// </summary>
		/// <param name="jobId">Id of the job to find</param>
		/// <param name="labelIndex">Index for the label</param>
		/// <returns>Notification info for the requested label in the job</returns>
		[HttpGet]
		[Route("/api/v1/jobs/{jobId}/labels/{labelIndex}/notifications")]
		public async Task<ActionResult<GetNotificationResponse>> GetLabelNotificationsAsync(JobId jobId, int labelIndex)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}

			INotificationSubscription? subscription;
			if (!job.LabelIdxToTriggerId.ContainsKey(labelIndex))
			{
				subscription = null;
			}
			else
			{
				subscription = await _notificationService.GetSubscriptionsAsync(job.LabelIdxToTriggerId[labelIndex], User);
			}
			return new GetNotificationResponse(subscription);
		}
	}
}
