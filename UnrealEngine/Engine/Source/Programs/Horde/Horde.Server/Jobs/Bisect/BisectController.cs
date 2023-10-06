// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Horde.Server.Acls;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Perforce;
using Horde.Server.Server;
using Horde.Server.Streams;
using Horde.Server.Users;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Components.Web;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;

namespace Horde.Server.Jobs.Bisect
{
	/// <summary>
	/// Request to create a new bisect task
	/// </summary>
	public class CreateBisectTaskRequest
	{
		/// <summary>
		/// Job containing the node to check
		/// </summary>
		public JobId JobId { get; set; }

		/// <summary>
		/// Name of the node to query
		/// </summary>
		public string NodeName { get; set; } = String.Empty;

		/// <summary>
		/// Commit tag to filter possible changes against
		/// </summary>
		public List<CommitTag>? CommitTags { get; set; }

		/// <summary>
		/// Set of changes to ignore. Can be modified later through <see cref="UpdateBisectTaskRequest"/>.
		/// </summary>
		public List<int>? IgnoreChanges { get; set; }

		/// <summary>
		/// Set of jobs to ignore. Can be modified later through <see cref="UpdateBisectTaskRequest"/>.
		/// </summary>
		public List<JobId>? IgnoreJobs { get; set; }
	}

	/// <summary>
	/// Response from creating a bisect task
	/// </summary>
	public class CreateBisectTaskResponse
	{
		/// <summary>
		/// Identifier for the new bisect task
		/// </summary>
		public BisectTaskId BisectTaskId { get; }

		internal CreateBisectTaskResponse(IBisectTask bisectTask) => BisectTaskId = bisectTask.Id;
	}

	/// <summary>
	/// Information about a bisect task
	/// </summary>
	public class GetBisectTaskResponse
	{
		readonly IBisectTask _bisectTask;

		/// <inheritdoc cref="IBisectTask.Id"/>
		public BisectTaskId Id => _bisectTask.Id;

		/// <inheritdoc cref="IBisectTask.State"/>
		public BisectTaskState State => _bisectTask.State;

		/// <inheritdoc cref="IBisectTask.OwnerId"/>
		public GetThinUserInfoResponse Owner { get; }

		/// <inheritdoc cref="IBisectTask.StreamId"/>
		public StreamId StreamId => _bisectTask.StreamId;

		/// <inheritdoc cref="IBisectTask.TemplateId"/>
		public TemplateId TemplateId => _bisectTask.TemplateId;

		/// <inheritdoc cref="IBisectTask.NodeName"/>
		public string NodeName => _bisectTask.NodeName;

		/// <inheritdoc cref="IBisectTask.Outcome"/>
		public JobStepOutcome Outcome => _bisectTask.Outcome;

		/// <inheritdoc cref="IBisectTask.InitialJobId"/>
		public JobId InitialJobId => _bisectTask.InitialJobId;

		/// <inheritdoc cref="IBisectTask.InitialChange"/>
		public int InitialChange => _bisectTask.InitialChange;

		/// <inheritdoc cref="IBisectTask.CurrentJobId"/>
		public JobId CurrentJobId => _bisectTask.CurrentJobId;

		/// <inheritdoc cref="IBisectTask.CurrentChange"/>
		public int CurrentChange => _bisectTask.CurrentChange;

		/// <summary>
		/// The steps involved in the bisection
		/// </summary>
		public List<GetJobStepRefResponse> Steps { get; }

		internal GetBisectTaskResponse(IBisectTask bisectTask, GetThinUserInfoResponse owner, List<IJobStepRef> steps)
		{
			_bisectTask = bisectTask;
			Owner = owner;
			Steps = steps.Select(s => new GetJobStepRefResponse(s)).ToList();

		}
	}

	/// <summary>
	/// Updates the state of a bisect task
	/// </summary>
	public class UpdateBisectTaskRequest
	{
		/// <summary>
		/// Cancels the current task
		/// </summary>
		public bool? Cancel { get; set; }

		/// <summary>
		/// List of change numbers to include in the search. 
		/// </summary>
		public List<int> IncludeChanges { get; set; } = new List<int>();

		/// <summary>
		/// List of change numbers to exclude from the search.
		/// </summary>
		public List<int> ExcludeChanges { get; set; } = new List<int>();

		/// <summary>
		/// List of jobs to include in the search.
		/// </summary>
		public List<JobId> IncludeJobs { get; set; } = new List<JobId>();

		/// <summary>
		/// List of jobs to exclude from the search.
		/// </summary>
		public List<JobId> ExcludeJobs { get; set; } = new List<JobId>();
	}


	/// <summary>
	/// Controller for the /api/v1/bisect endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class BisectTasksController : HordeControllerBase
	{
		readonly IBisectTaskCollection _bisectTaskCollection;
		readonly IJobCollection _jobCollection;
		readonly IJobStepRefCollection _jobStepRefs;
		readonly IGraphCollection _graphCollection;
		readonly IUserCollection _userCollection;
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public BisectTasksController(IBisectTaskCollection bisectTaskCollection, IJobCollection jobCollection, IJobStepRefCollection jobStepRefs, IGraphCollection graphCollection, IUserCollection userCollection, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_bisectTaskCollection = bisectTaskCollection;
			_jobCollection = jobCollection;
			_jobStepRefs = jobStepRefs;
			_graphCollection = graphCollection;
			_userCollection = userCollection;
			_globalConfig = globalConfig;
		}

		/// <summary>
		/// Creates a new bisect task
		/// </summary>
		/// <param name="create">Properties of the new bisect task</param>
		/// <param name="cancellationToken">Cancellation token for the request</param>
		/// <returns>Id of the new bisect task</returns>
		[HttpPost]
		[Route("/api/v1/bisect")]
		public async Task<ActionResult<CreateBisectTaskResponse>> CreateAsync([FromBody] CreateBisectTaskRequest create, CancellationToken cancellationToken = default)
		{
			IJob? job = await _jobCollection.GetAsync(create.JobId);
			if (job == null)
			{
				return NotFound(create.JobId);
			}

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out streamConfig))
			{
				return NotFound(job.StreamId);
			}
			if (!streamConfig.Authorize(JobAclAction.ViewJob, User))
			{
				return Forbid(JobAclAction.ViewJob, create.JobId);
			}
			if (!streamConfig.Authorize(BisectTaskAclAction.CreateBisectTask, User))
			{
				return Forbid(BisectTaskAclAction.CreateBisectTask, streamConfig.Id);
			}

			IGraph graph = await _graphCollection.GetAsync(job.GraphHash);

			NodeRef nodeRef;
			if (!graph.TryFindNode(create.NodeName, out nodeRef))
			{
				return BadRequest($"Graph does not contain '{create.NodeName}'");
			}

			IJobStep? jobStep;
			if (!job.TryGetStepForNode(nodeRef, out jobStep))
			{
				return BadRequest($"Job does not contain '{create.NodeName}'");
			}
			if (jobStep.IsPending())
			{
				return BadRequest("Step has not finished executing yet");
			}
			if (jobStep.Outcome == JobStepOutcome.Success)
			{
				return BadRequest("Step has not failed");
			}

			CreateBisectTaskOptions options = new CreateBisectTaskOptions();
			options.CommitTags = create.CommitTags;
			options.IgnoreChanges = create.IgnoreChanges;
			options.IgnoreJobs = create.IgnoreJobs;

			IBisectTask bisectTask = await _bisectTaskCollection.CreateAsync(job, create.NodeName, jobStep.Outcome, User.GetUserId() ?? UserId.Empty, options, cancellationToken);
			return new CreateBisectTaskResponse(bisectTask);
		}

		/// <summary>
		/// Gets information about a bisection task
		/// </summary>
		/// <param name="bisectTaskId">Id of the bisect task to retrieve</param>
		/// <param name="cancellationToken">Cancellation token for the request</param>
		/// <returns>Id of the new bisect task</returns>
		[HttpGet]
		[Route("/api/v1/bisect/{bisectTaskId}")]
		public async Task<ActionResult<GetBisectTaskResponse>> GetAsync([FromRoute] BisectTaskId bisectTaskId, CancellationToken cancellationToken = default)
		{
			IBisectTask? bisectTask = await _bisectTaskCollection.GetAsync(bisectTaskId, cancellationToken);
			if (bisectTask == null)
			{
				return NotFound(bisectTaskId);
			}

			IJob? initialJob = await _jobCollection.GetAsync(bisectTask.InitialJobId);
			if (initialJob == null)
			{
				return NotFound(bisectTask.InitialJobId);
			}

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(bisectTask.StreamId, out streamConfig))
			{
				return NotFound(bisectTask.StreamId);
			}
			if (!streamConfig.Authorize(BisectTaskAclAction.ViewBisectTask, User))
			{
				return Forbid(BisectTaskAclAction.ViewBisectTask, streamConfig.Id);
			}

			IUser? user = await _userCollection.GetUserAsync(bisectTask.OwnerId);
			List<IJobStepRef> steps = await _jobStepRefs.GetStepsForNodeAsync(initialJob.StreamId, initialJob.TemplateId, bisectTask.NodeName, null, true, 1024, bisectTask.Id, cancellationToken);

			return new GetBisectTaskResponse(bisectTask, new GetThinUserInfoResponse(user), steps);
		}

		/// <summary>
		/// Creates a new bisect task
		/// </summary>
		/// <param name="bisectTaskId">Id of the bisect task to retrieve</param>
		/// <param name="request">Updates for the task</param>
		/// <param name="cancellationToken">Cancellation token for the request</param>
		/// <returns>Id of the new bisect task</returns>
		[HttpPatch]
		[Route("/api/v1/bisect/{bisectTaskId}")]
		public async Task<ActionResult> UpdateAsync([FromRoute] BisectTaskId bisectTaskId, [FromBody] UpdateBisectTaskRequest request, CancellationToken cancellationToken)
		{
			for(; ;)
			{
				IBisectTask? bisectTask = await _bisectTaskCollection.GetAsync(bisectTaskId, cancellationToken);
				if (bisectTask == null)
				{
					return NotFound(bisectTaskId);
				}

				StreamConfig? streamConfig;
				if (!_globalConfig.Value.TryGetStream(bisectTask.StreamId, out streamConfig))
				{
					return NotFound(bisectTask.StreamId);
				}
				if (!streamConfig.Authorize(BisectTaskAclAction.UpdateBisectTask, User))
				{
					return Forbid(BisectTaskAclAction.UpdateBisectTask, streamConfig.Id);
				}

				UpdateBisectTaskOptions options = new UpdateBisectTaskOptions();
				options.State = (!request.Cancel.HasValue) ? null : request.Cancel.Value ? BisectTaskState.Cancelled : BisectTaskState.Running;
				options.IncludeChanges = request.IncludeChanges;
				options.ExcludeChanges = request.ExcludeChanges;
				options.IncludeJobs = request.IncludeJobs;
				options.ExcludeJobs = request.ExcludeJobs;

				IBisectTask? updatedTask = await _bisectTaskCollection.TryUpdateAsync(bisectTask, options, cancellationToken);
				if (updatedTask != null)
				{
					return Ok();
				}
			}
		}

		/// <summary>
		/// Gets the bisections run on a specific job
		/// </summary>
		/// <param name="jobId">Id of the job to retrieve</param>		
		/// <param name="cancellationToken" />
		/// <returns>Id of the new bisect task</returns>
		[HttpGet]
		[Route("/api/v1/bisect/job/{jobId}")]
		public async Task<ActionResult<List<GetBisectTaskResponse>>> GetJobBisectTasksAsync([FromRoute] JobId jobId, CancellationToken cancellationToken = default)
		{
			IJob? job = await _jobCollection.GetAsync(jobId);
			if (job == null)
			{
				return NotFound(jobId);
			}

			IReadOnlyList<IBisectTask> tasks = await _bisectTaskCollection.FindAsync(jobId, cancellationToken);

			List<GetBisectTaskResponse> response = new List<GetBisectTaskResponse>();

			foreach (IBisectTask bisectTask in tasks)
			{
				StreamConfig? streamConfig;
				if (!_globalConfig.Value.TryGetStream(bisectTask.StreamId, out streamConfig))
				{
					return NotFound(bisectTask.StreamId);
				}
				if (!streamConfig.Authorize(BisectTaskAclAction.ViewBisectTask, User))
				{
					return Forbid(BisectTaskAclAction.ViewBisectTask, streamConfig.Id);
				}



				IUser? user = await _userCollection.GetUserAsync(bisectTask.OwnerId);
				List<IJobStepRef> steps = await _jobStepRefs.GetStepsForNodeAsync(job.StreamId, job.TemplateId, bisectTask.NodeName, null, true, 1024, bisectTask.Id, cancellationToken);

				response.Add(new GetBisectTaskResponse(bisectTask, new GetThinUserInfoResponse(user), steps));
				
			}

			return response;
		}
	}

}
