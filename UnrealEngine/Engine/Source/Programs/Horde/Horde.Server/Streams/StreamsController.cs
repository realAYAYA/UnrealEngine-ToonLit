// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Jobs.Templates;
using EpicGames.Horde.Projects;
using EpicGames.Horde.Streams;
using EpicGames.Horde.Users;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Templates;
using Horde.Server.Perforce;
using Horde.Server.Projects;
using Horde.Server.Server;
using Horde.Server.Users;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;
using OpenTelemetry.Trace;
using TimeZoneConverter;

namespace Horde.Server.Streams
{
	/// <summary>
	/// Controller for the /api/v1/streams endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class StreamsController : HordeControllerBase
	{
		private readonly IStreamCollection _streamCollection;
		private readonly ICommitService _commitService;
		private readonly ITemplateCollection _templateCollection;
		private readonly IJobStepRefCollection _jobStepRefCollection;
		private readonly IUserCollection _userCollection;
		private readonly Tracer _tracer;
		private readonly IOptionsSnapshot<GlobalConfig> _globalConfig;
		private readonly TimeZoneInfo _timeZone;

		/// <summary>
		/// Constructor
		/// </summary>
		public StreamsController(IStreamCollection streamCollection, ICommitService commitService, ITemplateCollection templateCollection, IJobStepRefCollection jobStepRefCollection, IUserCollection userCollection, Tracer tracer, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_streamCollection = streamCollection;
			_commitService = commitService;
			_templateCollection = templateCollection;
			_jobStepRefCollection = jobStepRefCollection;
			_userCollection = userCollection;
			_tracer = tracer;
			_globalConfig = globalConfig;

			string? timeZoneName = _globalConfig.Value.ServerSettings.ScheduleTimeZone;
			_timeZone = (timeZoneName == null) ? TimeZoneInfo.Local : TZConvert.GetTimeZoneInfo(timeZoneName);
		}

		/// <summary>
		/// Query all the streams for a particular project.
		/// </summary>
		/// <param name="projectIds">Unique id of the project to query</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about all the projects</returns>
		[HttpGet]
		[Route("/api/v1/streams")]
		[ProducesResponseType(typeof(List<GetStreamResponse>), 200)]
		public async Task<ActionResult<List<object>>> GetStreamsAsync([FromQuery(Name = "ProjectId")] string[] projectIds, [FromQuery] PropertyFilter? filter = null, CancellationToken cancellationToken = default)
		{
			ProjectId[] projectIdValues = Array.ConvertAll(projectIds, x => new ProjectId(x));

			List<StreamConfig> streamConfigs = new List<StreamConfig>();
			foreach (ProjectConfig projectConfig in _globalConfig.Value.Projects)
			{
				if (projectIdValues.Length == 0 || projectIdValues.Contains(projectConfig.Id))
				{
					foreach (StreamConfig streamConfig in projectConfig.Streams)
					{
						if (streamConfig.Authorize(StreamAclAction.ViewStream, User))
						{
							streamConfigs.Add(streamConfig);
						}
					}
				}
			}

			IReadOnlyList<IStream> streams = await _streamCollection.GetAsync(streamConfigs, cancellationToken);

			List<GetStreamResponse> responses = new List<GetStreamResponse>();
			foreach (IStream stream in streams)
			{
				GetStreamResponse response = await CreateGetStreamResponseAsync(stream, cancellationToken);
				responses.Add(response);
			}

			return responses.OrderBy(x => x.Id).Select(x => PropertyFilter.Apply(x, filter)).ToList();
		}

		/// <summary>
		/// Query all the streams for a particular project.
		/// </summary>
		/// <param name="projectIds">Unique id of the project to query</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>Information about all the projects</returns>
		[HttpGet]
		[Route("/api/v2/streams")]
		[ProducesResponseType(typeof(List<GetStreamResponse>), 200)]
		public async Task<ActionResult<List<object>>> GetStreamsV2Async([FromQuery(Name = "ProjectId")] string[] projectIds, [FromQuery] PropertyFilter? filter = null)
		{
			ProjectId[] projectIdValues = Array.ConvertAll(projectIds, x => new ProjectId(x));

			List<GetStreamResponseV2> responses = new List<GetStreamResponseV2>();
			foreach (ProjectConfig projectConfig in _globalConfig.Value.Projects)
			{
				if (projectIdValues.Length == 0 || projectIdValues.Contains(projectConfig.Id))
				{
					foreach (StreamConfig streamConfig in projectConfig.Streams)
					{
						if (streamConfig.Authorize(StreamAclAction.ViewStream, User))
						{
							IStream stream = await _streamCollection.GetAsync(streamConfig);
							GetStreamResponseV2 response = await CreateGetStreamResponseV2Async(stream, false);
							responses.Add(response);
						}
					}
				}
			}

			return responses.OrderBy(x => x.Id).Select(x => PropertyFilter.Apply(x, filter)).ToList();
		}

		/// <summary>
		/// Retrieve information about a specific stream.
		/// </summary>
		/// <param name="streamId">Id of the stream to get information about</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about the requested project</returns>
		[HttpGet]
		[Route("/api/v1/streams/{streamId}")]
		[ProducesResponseType(typeof(GetStreamResponse), 200)]
		public async Task<ActionResult<object>> GetStreamAsync(StreamId streamId, [FromQuery] PropertyFilter? filter = null, CancellationToken cancellationToken = default)
		{
			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(streamId, out streamConfig))
			{
				return NotFound(streamId);
			}
			if (!streamConfig.Authorize(StreamAclAction.ViewStream, User))
			{
				return Forbid(StreamAclAction.ViewStream, streamId);
			}

			IStream stream = await _streamCollection.GetAsync(streamConfig, cancellationToken);
			return PropertyFilter.Apply(await CreateGetStreamResponseAsync(stream, cancellationToken), filter);
		}

		/// <summary>
		/// Retrieve information about a specific stream.
		/// </summary>
		/// <param name="streamId">Id of the stream to get information about</param>
		/// <param name="config">The client's cached config revision. If this matches value matches the current config version of the stream, the config object will be omitted from the response.</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>Information about the requested project</returns>
		[HttpGet]
		[Route("/api/v2/streams/{streamId}/config")]
		[ProducesResponseType(typeof(GetStreamResponseV2), 200)]
		public async Task<ActionResult<object>> GetStreamV2Async(StreamId streamId, [FromQuery] string? config = null, [FromQuery] PropertyFilter? filter = null)
		{
			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(streamId, out streamConfig))
			{
				return NotFound(streamId);
			}
			if (!streamConfig.Authorize(StreamAclAction.ViewStream, User))
			{
				return Forbid(StreamAclAction.ViewStream, streamId);
			}

			bool includeConfig = !String.Equals(config, streamConfig.Revision, StringComparison.Ordinal);
			IStream stream = await _streamCollection.GetAsync(streamConfig);
			return PropertyFilter.Apply(await CreateGetStreamResponseV2Async(stream, includeConfig), filter);
		}

		/// <summary>
		/// Create a stream response object, including all the templates
		/// </summary>
		/// <param name="stream">Stream to create response for</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Response object</returns>
		async Task<GetStreamResponse> CreateGetStreamResponseAsync(IStream stream, CancellationToken cancellationToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(StreamsController)}.{nameof(CreateGetStreamResponseAsync)}");
			span.SetAttribute("streamId", stream.Id);

			List<GetTemplateRefResponse> apiTemplateRefs = new List<GetTemplateRefResponse>();
			foreach (KeyValuePair<TemplateId, ITemplateRef> pair in stream.Templates)
			{
				using TelemetrySpan templateScope = _tracer.StartActiveSpan($"{nameof(StreamsController)}.{nameof(CreateGetStreamResponseAsync)}.Template");
				templateScope.SetAttribute("templateName", pair.Value.Config.Name);

				ITemplateRef templateRef = pair.Value;
				if (templateRef.Config.Authorize(StreamAclAction.ViewTemplate, User))
				{
					List<GetTemplateStepStateResponse>? stepStates = null;
					if (templateRef.StepStates != null)
					{
						for (int i = 0; i < templateRef.StepStates.Count; i++)
						{
							ITemplateStep state = templateRef.StepStates[i];

							stepStates ??= new List<GetTemplateStepStateResponse>();

							GetThinUserInfoResponse? pausedByUserInfo = (await _userCollection.GetCachedUserAsync(state.PausedByUserId, cancellationToken))?.ToThinApiResponse();
							stepStates.Add(new GetTemplateStepStateResponse(state, pausedByUserInfo));
						}
					}

					ITemplate? template = await _templateCollection.GetOrAddAsync(pair.Value.Config);
					apiTemplateRefs.Add(new GetTemplateRefResponse(pair.Key, pair.Value, template, stepStates, _timeZone));
				}
			}

			return stream.ToApiResponse(apiTemplateRefs);
		}

		async Task<GetStreamResponseV2> CreateGetStreamResponseV2Async(IStream stream, bool includeConfig)
		{
			List<GetTemplateRefResponseV2> templates = new List<GetTemplateRefResponseV2>();
			foreach (ITemplateRef templateRef in stream.Templates.Values)
			{
				ITemplate template = await _templateCollection.GetOrAddAsync(templateRef.Config);
				templates.Add(new GetTemplateRefResponseV2(template, templateRef));
			}
			return new GetStreamResponseV2(stream, includeConfig, templates);
		}

		/// <summary>
		/// Gets a list of changes for a stream
		/// </summary>
		/// <param name="streamId">The stream id</param>
		/// <param name="min">The starting changelist number</param>
		/// <param name="max">The ending changelist number</param>
		/// <param name="results">Number of results to return</param>
		/// <param name="tags">Tags to filter the changes returned</param>
		/// <param name="filter">The filter to apply to the results</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Http result code</returns>
		[HttpGet]
		[Route("/api/v1/streams/{streamId}/changes")]
		[ProducesResponseType(typeof(List<GetCommitResponse>), 200)]
		public async Task<ActionResult<List<object>>> GetChangesAsync(StreamId streamId, [FromQuery] int? min = null, [FromQuery] int? max = null, [FromQuery] int results = 50, [FromQuery] string? tags = null, PropertyFilter? filter = null, CancellationToken cancellationToken = default)
		{
			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(streamId, out streamConfig))
			{
				return NotFound(streamId);
			}
			if (!streamConfig.Authorize(StreamAclAction.ViewChanges, User))
			{
				return Forbid(StreamAclAction.ViewChanges, streamId);
			}

			List<CommitTag>? commitTags = null;
			if (tags != null)
			{
				commitTags = tags.Split(';', StringSplitOptions.RemoveEmptyEntries).Select(x => new CommitTag(x)).ToList();
			}

			List<ICommit> commits = await _commitService.GetCollection(streamConfig).FindAsync(min, max, results, commitTags, cancellationToken).ToListAsync(cancellationToken);

			List<GetCommitResponse> responses = new List<GetCommitResponse>();
			foreach (ICommit commit in commits)
			{
				IUser? author = await _userCollection.GetCachedUserAsync(commit.AuthorId, cancellationToken);
				responses.Add(new GetCommitResponse(commit, author!, null, null));
			}
			return responses.ConvertAll(x => PropertyFilter.Apply(x, filter));
		}

		/// <summary>
		/// Gets a list of changes for a stream
		/// </summary>
		/// <param name="streamId">The stream id</param>
		/// <param name="changeNumber">The changelist number</param>
		/// <param name="maxFiles">Maximum number of files to return</param>
		/// <param name="filter">The filter to apply to the results</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Http result code</returns>
		[HttpGet]
		[Route("/api/v1/streams/{streamId}/changes/{changeNumber}")]
		[ProducesResponseType(typeof(GetCommitResponse), 200)]
		public async Task<ActionResult<object>> GetChangeDetailsAsync(StreamId streamId, int changeNumber, int maxFiles = 100, PropertyFilter? filter = null, CancellationToken cancellationToken = default)
		{
			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(streamId, out streamConfig))
			{
				return NotFound(streamId);
			}
			if (!streamConfig.Authorize(StreamAclAction.ViewChanges, User))
			{
				return Forbid(StreamAclAction.ViewChanges, streamId);
			}

			ICommit? changeDetails = await _commitService.GetCollection(streamConfig).GetAsync(changeNumber, cancellationToken);
			if (changeDetails == null)
			{
				return NotFound("CL {Change} not found in stream {StreamId}", changeNumber, streamId);
			}

			IUser? author = await _userCollection.GetCachedUserAsync(changeDetails.AuthorId, cancellationToken);
			IReadOnlyList<CommitTag> tags = await changeDetails.GetTagsAsync(cancellationToken);
			IReadOnlyList<string> files = await changeDetails.GetFilesAsync(maxFiles, cancellationToken);

			return PropertyFilter.Apply(new GetCommitResponse(changeDetails, author!, tags, files), filter);
		}

		/// <summary>
		/// Gets the history of a step in the stream
		/// </summary>
		/// <param name="streamId">The stream id</param>
		/// <param name="templateId"></param>
		/// <param name="step">Name of the step to search for</param>
		/// <param name="change">Maximum changelist number to return</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="filter">The filter to apply to the results</param>
		/// <returns>Http result code</returns>
		[HttpGet]
		[Route("/api/v1/streams/{streamId}/history")]
		[ProducesResponseType(typeof(List<GetJobStepRefResponse>), 200)]
		public async Task<ActionResult<List<object>>> GetStepHistoryAsync(StreamId streamId, [FromQuery] string templateId, [FromQuery] string step, [FromQuery] int? change = null, [FromQuery] int count = 10, [FromQuery] PropertyFilter? filter = null)
		{
			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(streamId, out streamConfig))
			{
				return NotFound(streamId);
			}
			if (!streamConfig.Authorize(JobAclAction.ViewJob, User))
			{
				return Forbid(JobAclAction.ViewJob, streamId);
			}

			TemplateId templateIdValue = new TemplateId(templateId);

			List<IJobStepRef> steps = await _jobStepRefCollection.GetStepsForNodeAsync(streamId, templateIdValue, step, change, true, count);
			return steps.ConvertAll(x => PropertyFilter.Apply(new GetJobStepRefResponse(x), filter));
		}

		/// <summary>
		/// Gets a template for a stream
		/// </summary>
		/// <param name="streamId">Unique id of the stream to query</param>
		/// <param name="templateId">Unique id of the template to query</param>
		/// <param name="filter">Filter for properties to return</param>
		/// <returns>Information about all the templates</returns>
		[HttpGet]
		[Route("/api/v1/streams/{streamId}/templates/{templateId}")]
		[ProducesResponseType(typeof(List<GetTemplateResponse>), 200)]
		public async Task<ActionResult<object>> GetTemplateAsync(StreamId streamId, TemplateId templateId, [FromQuery] PropertyFilter? filter = null)
		{
			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(streamId, out streamConfig))
			{
				return NotFound(streamId);
			}

			TemplateRefConfig? templateConfig;
			if (!streamConfig.TryGetTemplate(templateId, out templateConfig))
			{
				return NotFound(streamId, templateId);
			}
			if (!templateConfig.Authorize(StreamAclAction.ViewTemplate, User))
			{
				return Forbid(StreamAclAction.ViewTemplate, streamId);
			}

			ITemplate template = await _templateCollection.GetOrAddAsync(templateConfig);
			return new GetTemplateResponse(template).ApplyFilter(filter);
		}

		/// <summary>
		/// Update a stream template ref
		/// </summary>
		[HttpPut]
		[Authorize]
		[Route("/api/v1/streams/{streamId}/templates/{templateRefId}")]
		public async Task<ActionResult> UpdateStreamTemplateRefAsync(StreamId streamId, TemplateId templateRefId, [FromBody] UpdateTemplateRefRequest update)
		{
			if (!_globalConfig.Value.Authorize(AdminAclAction.AdminWrite, User))
			{
				return Forbid(AdminAclAction.AdminWrite);
			}

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(streamId, out streamConfig))
			{
				return NotFound(streamId);
			}

			IStream stream = await _streamCollection.GetAsync(streamConfig);
			if (!stream.Templates.ContainsKey(templateRefId))
			{
				return NotFound(streamId, templateRefId);
			}

			await _streamCollection.TryUpdateTemplateRefAsync(stream, templateRefId, update.StepStates);
			return Ok();
		}
	}
}
