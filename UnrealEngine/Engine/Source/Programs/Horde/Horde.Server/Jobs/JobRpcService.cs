// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Net.Http;
using System.Security.Claims;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Agents.Sessions;
using EpicGames.Horde.Artifacts;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Streams;
using Google.Protobuf.WellKnownTypes;
using Grpc.Core;
using Horde.Common.Rpc;
using Horde.Server.Acls;
using Horde.Server.Artifacts;
using Horde.Server.Jobs.Artifacts;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Jobs.Templates;
using Horde.Server.Jobs.TestData;
using Horde.Server.Logs;
using Horde.Server.Server;
using Horde.Server.Streams;
using Horde.Server.Utilities;
using HordeCommon;
using HordeCommon.Rpc;
using Microsoft.AspNetCore.Authorization;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using MongoDB.Bson;
using MongoDB.Bson.Serialization;

namespace Horde.Server.Jobs
{
	using RpcGetJobResponse = HordeCommon.Rpc.GetJobResponse;
	using RpcGetStepResponse = HordeCommon.Rpc.GetStepResponse;
	using RpcGetStreamResponse = HordeCommon.Rpc.GetStreamResponse;
	using RpcUpdateJobRequest = HordeCommon.Rpc.UpdateJobRequest;
	using RpcUpdateStepRequest = HordeCommon.Rpc.UpdateStepRequest;

	/// <summary>
	/// Implements the Horde gRPC service for bots updating their status and dequeing work
	/// </summary>
	[Authorize]
	public class JobRpcService : JobRpc.JobRpcBase
	{
		readonly AclService _aclService;
		readonly IJobCollection _jobCollection;
		readonly IArtifactCollection _artifactCollection;
		readonly JobRpcCommon _jobRpcCommon;
		readonly GlobalConfig _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public JobRpcService(AclService aclService, IJobCollection jobCollection, IArtifactCollection artifactCollection, JobRpcCommon jobRpcCommon, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_aclService = aclService;
			_jobCollection = jobCollection;
			_artifactCollection = artifactCollection;
			_jobRpcCommon = jobRpcCommon;
			_globalConfig = globalConfig.Value;
		}

		/// <inheritdoc/>
		public override async Task<Common.Rpc.CreateJobArtifactResponse> CreateArtifact(CreateJobArtifactRequest request, ServerCallContext context)
		{
			(IJob job, _, IJobStep step) = await AuthorizeAsync(request.JobId, request.StepId, context);

			ArtifactType type = request.Type switch
			{
				JobArtifactType.TempStorage => ArtifactType.StepOutput,
				JobArtifactType.Saved => ArtifactType.StepSaved,
				JobArtifactType.Trace => ArtifactType.StepTrace,
				JobArtifactType.TestData => ArtifactType.StepTestData,
				_ => throw new StructuredRpcException(StatusCode.InvalidArgument, "Invalid artifact type")
			};

			List<string> keys = new List<string>();
			keys.Add($"job:{job.Id}");
			keys.Add($"job:{job.Id}/step:{step.Id}");

			List<string> metadata = new List<string>();

			if (!_globalConfig.TryGetTemplate(job.StreamId, job.TemplateId, out TemplateRefConfig? templateConfig))
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Couldn't find template {TemplateId} in stream {StreamId}", job.TemplateId, job.StreamId);
			}

			IArtifact artifact = await _artifactCollection.AddAsync(new ArtifactName("default"), type, null, job.StreamId, job.Change, keys, metadata, templateConfig.Acl.ScopeName, context.CancellationToken);

			List<AclClaimConfig> claims = new List<AclClaimConfig>();
			claims.Add(new AclClaimConfig(HordeClaimTypes.WriteNamespace, artifact.NamespaceId.ToString()));

			string token = await _aclService.IssueBearerTokenAsync(claims, TimeSpan.FromHours(8.0), context.CancellationToken);
			return new Common.Rpc.CreateJobArtifactResponse { Id = artifact.Id.ToString(), NamespaceId = artifact.NamespaceId.ToString(), RefName = artifact.RefName.ToString(), Token = token };
		}

		/// <inheritdoc/>
		public override async Task<Common.Rpc.CreateJobArtifactResponseV2> CreateArtifactV2(CreateJobArtifactRequestV2 request, ServerCallContext context)
		{
			(IJob job, _, IJobStep step) = await AuthorizeAsync(request.JobId, request.StepId, context);

			ArtifactName name = new ArtifactName(request.Name);
			ArtifactType type = new ArtifactType(request.Type);

			List<string> keys = new List<string>();
			keys.Add(job.GetArtifactKey());
			keys.Add(job.GetArtifactKey(step));
			keys.AddRange(request.Keys);

			if (!_globalConfig.TryGetTemplate(job.StreamId, job.TemplateId, out TemplateRefConfig? templateConfig))
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Couldn't find template {TemplateId} in stream {StreamId}", job.TemplateId, job.StreamId);
			}

			string? description = request.Description;
			if (String.IsNullOrEmpty(description))
			{
				description = request.Name;
			}

			IArtifact artifact = await _artifactCollection.AddAsync(name, type, description, job.StreamId, job.Change, keys, request.Metadata, templateConfig.Acl.ScopeName, context.CancellationToken);

			List<AclClaimConfig> claims = new List<AclClaimConfig>();
			claims.Add(new AclClaimConfig(HordeClaimTypes.WriteNamespace, $"{artifact.NamespaceId}:{artifact.RefName}"));

			string token = await _aclService.IssueBearerTokenAsync(claims, TimeSpan.FromHours(8.0), context.CancellationToken);

			Common.Rpc.CreateJobArtifactResponseV2 response = new Common.Rpc.CreateJobArtifactResponseV2();
			response.Id = artifact.Id.ToString();
			response.NamespaceId = artifact.NamespaceId.ToString();
			response.RefName = artifact.RefName.ToString();
			response.Token = token;
			return response;
		}

		/// <inheritdoc/>
		public override async Task<Common.Rpc.GetJobArtifactResponse> GetArtifact(GetJobArtifactRequest request, ServerCallContext context)
		{
			(IJob job, _, _) = await AuthorizeAsync(request.JobId, request.StepId, context);

			ArtifactName name = new ArtifactName(request.Name);
			ArtifactType type = new ArtifactType(request.Type);

			IArtifact? artifact = await _artifactCollection.FindAsync(name: name, type: type, keys: new[] { job.GetArtifactKey() }, cancellationToken: context.CancellationToken).FirstOrDefaultAsync(context.CancellationToken);
			if (artifact == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, "No artifact {ArtifactName} of type {ArtifactType} was found for job {JobId}", name, type, job.Id);
			}

			List<AclClaimConfig> claims = new List<AclClaimConfig>();
			claims.Add(new AclClaimConfig(HordeClaimTypes.ReadNamespace, $"{artifact.NamespaceId}:{artifact.RefName}"));

			string token = await _aclService.IssueBearerTokenAsync(claims, TimeSpan.FromHours(8.0), context.CancellationToken);

			Common.Rpc.GetJobArtifactResponse response = new Common.Rpc.GetJobArtifactResponse();
			response.Id = artifact.Id.ToString();
			response.NamespaceId = artifact.NamespaceId.ToString();
			response.RefName = artifact.RefName.ToString();
			response.Token = token;
			return response;
		}

		Task<(IJob, IJobStepBatch, IJobStep)> AuthorizeAsync(string jobId, string stepId, ServerCallContext context)
		{
			return AuthorizeAsync(JobId.Parse(jobId), JobStepId.Parse(stepId), context);
		}

		async Task<(IJob, IJobStepBatch, IJobStep)> AuthorizeAsync(JobId jobId, JobStepId stepId, ServerCallContext context)
		{
			IJob? job = await _jobCollection.GetAsync(jobId);
			if (job == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Unable to find job {JobId}", jobId);
			}
			if (!job.TryGetStep(stepId, out IJobStepBatch? batch, out IJobStep? step))
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Unable to find step {JobId}:{StepId}", job.Id, stepId);
			}
			if (batch.SessionId == null)
			{
				throw new StructuredRpcException(StatusCode.PermissionDenied, "Batch {JobId}:{BatchId} has no session id", job.Id, batch.Id);
			}
			if (batch.LeaseId == null)
			{
				throw new StructuredRpcException(StatusCode.PermissionDenied, "Batch {JobId}:{BatchId} has no lease id", job.Id, batch.Id);
			}

			ClaimsPrincipal principal = context.GetHttpContext().User;
			if (!principal.HasSessionClaim(batch.SessionId.Value) && !principal.HasLeaseClaim(batch.LeaseId.Value))
			{
				throw new StructuredRpcException(StatusCode.PermissionDenied, "Session id {SessionId} not valid for step {JobId}:{BatchId}:{StepId}. Expected {ExpectedSessionId}.", principal.GetSessionClaim() ?? SessionId.Empty, job.Id, batch.Id, step.Id, batch.SessionId.Value);
			}

			return (job, batch, step);
		}

		/// <inheritdoc/>
		public override Task<RpcGetStreamResponse> GetStream(GetStreamRequest request, ServerCallContext context) => _jobRpcCommon.GetStreamAsync(request, context);

		/// <inheritdoc/>
		public override Task<RpcGetJobResponse> GetJob(GetJobRequest request, ServerCallContext context) => _jobRpcCommon.GetJobAsync(request, context);

		/// <inheritdoc/>
		public override Task<Empty> UpdateJob(RpcUpdateJobRequest request, ServerCallContext context) => _jobRpcCommon.UpdateJobAsync(request, context);

		/// <inheritdoc/>
		public override Task<BeginBatchResponse> BeginBatch(BeginBatchRequest request, ServerCallContext context) => _jobRpcCommon.BeginBatchAsync(request, context);

		/// <inheritdoc/>
		public override Task<Empty> FinishBatch(FinishBatchRequest request, ServerCallContext context) => _jobRpcCommon.FinishBatchAsync(request, context);

		/// <inheritdoc/>
		public override Task<BeginStepResponse> BeginStep(BeginStepRequest request, ServerCallContext context) => _jobRpcCommon.BeginStepAsync(request, context);

		/// <inheritdoc/>
		public override Task<Empty> UpdateStep(RpcUpdateStepRequest request, ServerCallContext context) => _jobRpcCommon.UpdateStepAsync(request, context);

		/// <inheritdoc/>
		public override Task<RpcGetStepResponse> GetStep(GetStepRequest request, ServerCallContext context) => _jobRpcCommon.GetStepAsync(request, context);

		/// <inheritdoc/>
		public override Task<UpdateGraphResponse> UpdateGraph(UpdateGraphRequest request, ServerCallContext context) => _jobRpcCommon.UpdateGraphAsync(request, context);

		/// <inheritdoc/>
		public override Task<Empty> CreateEvents(CreateEventsRequest request, ServerCallContext context) => _jobRpcCommon.CreateEventsAsync(request, context);

		/// <inheritdoc/>
		public override Task<UploadArtifactResponse> UploadArtifact(IAsyncStreamReader<UploadArtifactRequest> reader, ServerCallContext context) => _jobRpcCommon.UploadArtifactAsync(reader, context);

		/// <inheritdoc/>
		public override Task<UploadTestDataResponse> UploadTestData(IAsyncStreamReader<UploadTestDataRequest> reader, ServerCallContext context) => _jobRpcCommon.UploadTestDataAsync(reader, context);

		/// <inheritdoc/>
		public override Task<CreateReportResponse> CreateReport(CreateReportRequest request, ServerCallContext context) => _jobRpcCommon.CreateReportAsync(request, context);
	}

	/// <summary>
	/// Common methods between HordeRpc and JobRpc.
	/// </summary>
	public class JobRpcCommon
	{
		readonly JobService _jobService;
		readonly IArtifactCollectionV1 _artifactCollection;
		readonly ILogFileService _logFileService;
		readonly IGraphCollection _graphs;
		readonly ITestDataCollection _testData;
		readonly IJobStepRefCollection _jobStepRefCollection;
		readonly ITemplateCollection _templateCollection;
		readonly HttpClient _httpClient;
		readonly IClock _clock;
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public JobRpcCommon(JobService jobService, IArtifactCollectionV1 artifactCollection, ILogFileService logFileService, IGraphCollection graphs, ITestDataCollection testData, IJobStepRefCollection jobStepRefCollection, ITemplateCollection templateCollection, HttpClient httpClient, IClock clock, IOptionsSnapshot<GlobalConfig> globalConfig, ILogger<JobRpcCommon> logger)
		{
			_jobService = jobService;
			_artifactCollection = artifactCollection;
			_logFileService = logFileService;
			_graphs = graphs;
			_testData = testData;
			_jobStepRefCollection = jobStepRefCollection;
			_templateCollection = templateCollection;
			_httpClient = httpClient;
			_clock = clock;
			_globalConfig = globalConfig;
			_logger = logger;
		}

		/// <summary>
		/// Gets information about a stream
		/// </summary>
		/// <param name="request">Request arguments</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the new agent</returns>
		public Task<RpcGetStreamResponse> GetStreamAsync(GetStreamRequest request, ServerCallContext context)
		{
			StreamId streamIdValue = new StreamId(request.StreamId);

			StreamConfig? streamConfig;
			if (!_globalConfig.Value.TryGetStream(streamIdValue, out streamConfig))
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Stream {StreamId} does not exist", request.StreamId);
			}
			if (!streamConfig.Authorize(StreamAclAction.ViewStream, context.GetHttpContext().User))
			{
				throw new StructuredRpcException(StatusCode.PermissionDenied, "Not authenticated to access stream {StreamId}", request.StreamId);
			}

			return Task.FromResult(streamConfig.ToRpcResponse());
		}

		/// <summary>
		/// Gets information about a job
		/// </summary>
		/// <param name="request">Request arguments</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the new agent</returns>
		public async Task<RpcGetJobResponse> GetJobAsync(GetJobRequest request, ServerCallContext context)
		{
			JobId jobIdValue = JobId.Parse(request.JobId);

			IJob? job = await _jobService.GetJobAsync(jobIdValue);
			if (job == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Job {JobId} does not exist", request.JobId);
			}
			if (!JobService.AuthorizeSession(job, context.GetHttpContext().User))
			{
				throw new StructuredRpcException(StatusCode.PermissionDenied, "Not authenticated to access job {JobId}", request.JobId);
			}

			return job.ToRpcResponse();
		}

		/// <summary>
		/// Updates properties on a job
		/// </summary>
		/// <param name="request">Request arguments</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the new agent</returns>
		public async Task<Empty> UpdateJobAsync(RpcUpdateJobRequest request, ServerCallContext context)
		{
			JobId jobIdValue = JobId.Parse(request.JobId);

			IJob? job = await _jobService.GetJobAsync(jobIdValue);
			if (job == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Job {JobId} does not exist", request.JobId);
			}
			if (!JobService.AuthorizeSession(job, context.GetHttpContext().User))
			{
				throw new StructuredRpcException(StatusCode.PermissionDenied, "Not authenticated to modify job {JobId}", request.JobId);
			}

			await _jobService.UpdateJobAsync(job, name: request.Name);
			return new Empty();
		}

		/// <summary>
		/// Starts executing a batch
		/// </summary>
		/// <param name="request">Request arguments</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the new agent</returns>
		public async Task<BeginBatchResponse> BeginBatchAsync(BeginBatchRequest request, ServerCallContext context)
		{
			JobStepBatchId batchId = JobStepBatchId.Parse(request.BatchId);

			IJob? job = await _jobService.GetJobAsync(JobId.Parse(request.JobId));
			if (job == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Job {JobId} not found", request.JobId);
			}
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out StreamConfig? streamConfig))
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Stream {StreamId} not found", job.StreamId);
			}

			IJobStepBatch batch = AuthorizeBatch(job, JobStepBatchId.Parse(request.BatchId), context);
			job = await _jobService.UpdateBatchAsync(job, batchId, streamConfig, newState: JobStepBatchState.Starting);

			if (job == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Batch {JobId}:{BatchId} not found for updating", request.JobId, request.BatchId);
			}

			IGraph graph = await _jobService.GetGraphAsync(job);
			AgentConfig agentConfig = streamConfig.AgentTypes[graph.Groups[batch.GroupIdx].AgentType];

			BeginBatchResponse response = new BeginBatchResponse();
			response.LogId = batch.LogId.ToString();
			response.AgentType = graph.Groups[batch.GroupIdx].AgentType;
			response.StreamName = streamConfig.Name;
			response.Change = job.Change;
			response.CodeChange = job.CodeChange;
			response.PreflightChange = job.PreflightChange;
			response.ClonedPreflightChange = job.ClonedPreflightChange;
			response.Arguments.AddRange(job.Arguments);
			if (agentConfig.TempStorageDir != null)
			{
				response.TempStorageDir = agentConfig.TempStorageDir;
			}
			if (agentConfig.Environment != null)
			{
				response.Environment.Add(agentConfig.Environment);
			}
			response.ValidAgentTypes.Add(streamConfig.AgentTypes.Keys);

			return response;
		}

		/// <summary>
		/// Finishes executing a batch
		/// </summary>
		/// <param name="request">Request arguments</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the new agent</returns>
		public async Task<Empty> FinishBatchAsync(FinishBatchRequest request, ServerCallContext context)
		{
			IJob? job = await _jobService.GetJobAsync(JobId.Parse(request.JobId));
			if (job == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Job {JobId} not found", request.JobId);
			}
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out StreamConfig? streamConfig))
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Stream {StreamId} not found", job.StreamId);
			}

			IJobStepBatch batch = AuthorizeBatch(job, JobStepBatchId.Parse(request.BatchId), context);
			await _jobService.UpdateBatchAsync(job, batch.Id, streamConfig, newState: JobStepBatchState.Complete);
			return new Empty();
		}

		/// <summary>
		/// Starts executing a step
		/// </summary>
		/// <param name="request">Request arguments</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the new agent</returns>
		public async Task<BeginStepResponse> BeginStepAsync(BeginStepRequest request, ServerCallContext context)
		{
			Boxed<ILogFile?> log = new Boxed<ILogFile?>(null);
			for (; ; )
			{
				BeginStepResponse? response = await TryBeginStepAsync(request, log, context);
				if (response != null)
				{
					return response;
				}
			}
		}

		async Task<BeginStepResponse?> TryBeginStepAsync(BeginStepRequest request, Boxed<ILogFile?> log, ServerCallContext context)
		{
			// Check the job exists and we can access it
			IJob? job = await _jobService.GetJobAsync(JobId.Parse(request.JobId));
			if (job == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Job {JobId} not found", request.JobId);
			}
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out StreamConfig? streamConfig))
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Stream {StreamId} not found", job.StreamId);
			}

			// Find the batch being executed
			IJobStepBatch batch = AuthorizeBatch(job, JobStepBatchId.Parse(request.BatchId), context);
			if (batch.State != JobStepBatchState.Starting && batch.State != JobStepBatchState.Running)
			{
				return new BeginStepResponse { State = BeginStepResponse.Types.Result.Complete };
			}

			// Figure out which step to execute next
			IJobStep? step;
			for (int stepIdx = 0; ; stepIdx++)
			{
				// If there aren't any more steps, send a complete message
				if (stepIdx == batch.Steps.Count)
				{
					_logger.LogDebug("Job {JobId} batch {BatchId} is complete", job.Id, batch.Id);
					if (await _jobService.TryUpdateBatchAsync(job, batch.Id, newState: JobStepBatchState.Stopping) == null)
					{
						return null;
					}
					return new BeginStepResponse { State = BeginStepResponse.Types.Result.Complete };
				}

				// Check if this step is ready to be executed
				step = batch.Steps[stepIdx];
				if (step.State == JobStepState.Ready)
				{
					break;
				}
				if (step.State == JobStepState.Waiting)
				{
					_logger.LogDebug("Waiting for job {JobId}, batch {BatchId}, step {StepId}", job.Id, batch.Id, step.Id);
					return new BeginStepResponse { State = BeginStepResponse.Types.Result.Waiting };
				}
			}

			// Create a log file if necessary
			log.Value ??= await _logFileService.CreateLogFileAsync(job.Id, batch.LeaseId, batch.SessionId, LogType.Json);

			// Get the node for this step
			IGraph graph = await _jobService.GetGraphAsync(job);
			INode node = graph.Groups[batch.GroupIdx].Nodes[step.NodeIdx];

			// Figure out all the credentials for it (and check we can access them)
			Dictionary<string, string> credentials = new Dictionary<string, string>();
			//				if (Node.Credentials != null)
			//				{
			//					ClaimsPrincipal Principal = new ClaimsPrincipal(new ClaimsIdentity(Job.Claims.Select(x => new Claim(x.Type, x.Value))));
			//					if (!await GetCredentialsForStep(Principal, Node, Credentials, Message => FailStep(Job, Batch.Id, Step.Id, Log, Message)))
			//					{
			//						Log = null;
			//						continue;
			//					}
			//				}

			// Update the step state
			IJob? newJob = await _jobService.TryUpdateStepAsync(job, batch.Id, step.Id, streamConfig, JobStepState.Running, JobStepOutcome.Unspecified, newLogId: log.Value.Id);
			if (newJob != null)
			{
				BeginStepResponse response = new BeginStepResponse();
				response.State = BeginStepResponse.Types.Result.Ready;
				response.LogId = log.Value.Id.ToString();
				response.StepId = step.Id.ToString();
				response.Name = node.Name;
				response.Credentials.Add(credentials);

				foreach (NodeOutputRef input in node.Inputs)
				{
					INode inputNode = graph.GetNode(input.NodeRef);
					response.Inputs.Add($"{inputNode.Name}/{inputNode.OutputNames[input.OutputIdx]}");
				}

				response.OutputNames.Add(node.OutputNames);

				foreach (INodeGroup otherGroup in graph.Groups)
				{
					if (otherGroup != graph.Groups[batch.GroupIdx])
					{
						foreach (NodeOutputRef outputRef in otherGroup.Nodes.SelectMany(x => x.Inputs))
						{
							if (outputRef.NodeRef.GroupIdx == batch.GroupIdx && outputRef.NodeRef.NodeIdx == step.NodeIdx && !response.PublishOutputs.Contains(outputRef.OutputIdx))
							{
								response.PublishOutputs.Add(outputRef.OutputIdx);
							}
						}
					}
				}

				string templateName = "<unknown>";
				if (job.TemplateHash != null)
				{
					ITemplate? template = await _templateCollection.GetAsync(job.TemplateHash);
					templateName = template != null ? template.Name : templateName;
				}

				response.EnvVars.Add("UE_HORDE_TEMPLATEID", job.TemplateId.ToString());
				response.EnvVars.Add("UE_HORDE_TEMPLATENAME", templateName);
				response.EnvVars.Add("UE_HORDE_STEPNAME", node.Name);

				if (job.Environment != null)
				{
					response.EnvVars.Add(job.Environment);
				}

				IJobStepRef? lastStep = await _jobStepRefCollection.GetPrevStepForNodeAsync(job.StreamId, job.TemplateId, node.Name, job.Change);
				if (lastStep != null)
				{
					response.EnvVars.Add("UE_HORDE_LAST_CL", lastStep.Change.ToString(CultureInfo.InvariantCulture));

					int? lastSuccessChange = null;
					if (lastStep.Outcome == JobStepOutcome.Success)
					{
						lastSuccessChange = lastStep.Change;
					}
					else if (lastStep.LastSuccess != null)
					{
						lastSuccessChange = lastStep.LastSuccess.Value;
					}
					else
					{
						// Previous job hasn't finished yet; need to search for *current* last success step
						IJobStepRef? lastSuccess = await _jobStepRefCollection.GetPrevStepForNodeAsync(job.StreamId, job.TemplateId, node.Name, job.Change, outcome: JobStepOutcome.Success);
						if (lastSuccess != null)
						{
							lastSuccessChange = lastSuccess.Change;
						}
					}

					int? lastWarningChange = null;
					if (lastStep.Outcome == JobStepOutcome.Success || lastStep.Outcome == JobStepOutcome.Warnings)
					{
						lastWarningChange = lastStep.Change;
					}
					else if (lastStep.LastWarning != null)
					{
						lastWarningChange = lastStep.LastWarning.Value;
					}
					else
					{
						// Previous job hasn't finished yet; need to search for *current* last warning step
						IJobStepRef? lastWarnings = await _jobStepRefCollection.GetPrevStepForNodeAsync(job.StreamId, job.TemplateId, node.Name, job.Change, outcome: JobStepOutcome.Warnings);
						if (lastWarnings != null && (lastSuccessChange == null || lastWarnings.Change > lastSuccessChange.Value))
						{
							lastWarningChange = lastWarnings.Change;
						}
						else
						{
							lastWarningChange = lastSuccessChange;
						}
					}

					if (lastSuccessChange != null)
					{
						response.EnvVars.Add("UE_HORDE_LAST_SUCCESS_CL", lastSuccessChange.Value.ToString(CultureInfo.InvariantCulture));
					}
					if (lastWarningChange != null)
					{
						response.EnvVars.Add("UE_HORDE_LAST_WARNING_CL", lastWarningChange.Value.ToString(CultureInfo.InvariantCulture));
					}
				}

				List<TokenConfig> tokenConfigs = new List<TokenConfig>(streamConfig.Tokens);

				INodeGroup group = graph.Groups[batch.GroupIdx];
				if (streamConfig.AgentTypes.TryGetValue(group.AgentType, out AgentConfig? agentConfig) && agentConfig.Tokens != null)
				{
					tokenConfigs.AddRange(agentConfig.Tokens);
				}

				foreach (TokenConfig tokenConfig in tokenConfigs)
				{
					string? value = await AllocateTokenAsync(tokenConfig, context.CancellationToken);
					if (value == null)
					{
						_logger.LogWarning("Unable to allocate token for job {JobId}:{BatchId}:{StepId} from {Url}", job.Id, batch.Id, step.Id, tokenConfig.Url);
					}
					else
					{
						response.Credentials.Add(tokenConfig.EnvVar, value);
					}
				}

				if (node.Properties != null)
				{
					response.Properties.Add(node.Properties);
				}
				response.Warnings = node.Warnings;

				foreach (IGraphArtifact artifact in graph.Artifacts)
				{
					if (node.OutputNames.Contains(artifact.OutputName))
					{
						CreateGraphArtifactRequest stepArtifact = new CreateGraphArtifactRequest { Name = artifact.Name.ToString(), Type = artifact.Type.ToString(), Description = artifact.Description, BasePath = artifact.BasePath, OutputName = artifact.OutputName };
						stepArtifact.Keys.AddRange(artifact.Keys);
						stepArtifact.Metadata.AddRange(artifact.Metadata);
						response.Artifacts.Add(stepArtifact);
					}
				}

				return response;
			}

			return null;
		}

		class GetTokenResponse
		{
			[JsonPropertyName("token_type")]
			public string TokenType { get; set; } = String.Empty;

			[JsonPropertyName("access_token")]
			public string AccessToken { get; set; } = String.Empty;
		}

		async Task<string?> AllocateTokenAsync(TokenConfig config, CancellationToken cancellationToken)
		{
			List<KeyValuePair<string, string>> content = new List<KeyValuePair<string, string>>();
			content.Add(KeyValuePair.Create("grant_type", "client_credentials"));
			content.Add(KeyValuePair.Create("scope", "cache_access"));
			content.Add(KeyValuePair.Create("client_id", config.ClientId));
			content.Add(KeyValuePair.Create("client_secret", config.ClientSecret));

			using (HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, config.Url))
			{
				request.Content = new FormUrlEncodedContent(content);
				using (HttpResponseMessage response = await _httpClient.SendAsync(request, cancellationToken))
				{
					string text = await response.Content.ReadAsStringAsync(cancellationToken);
					if (!response.IsSuccessStatusCode)
					{
						throw new Exception($"Unexpected response while allocating token from {config.Url}: {text}");
					}

					try
					{
						return JsonSerializer.Deserialize<GetTokenResponse>(text)?.AccessToken;
					}
					catch (Exception ex)
					{
						throw new Exception($"Error allocating token from {config.Url}", ex);
					}
				}
			}
		}

		async Task<IJob> GetJobAsync(JobId jobId, CancellationToken cancellationToken)
		{
			IJob? job = await _jobService.GetJobAsync(jobId, cancellationToken);
			if (job == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Job {JobId} not found", jobId);
			}
			return job;
		}

		static IJobStepBatch AuthorizeBatch(IJob job, JobStepBatchId batchId, ServerCallContext context)
		{
			IJobStepBatch? batch;
			if (!job.TryGetBatch(batchId, out batch))
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Unable to find batch {JobId}:{BatchId}", job.Id, batchId);
			}
			if (batch.SessionId == null)
			{
				throw new StructuredRpcException(StatusCode.PermissionDenied, "Batch {JobId}:{BatchId} has no session id", job.Id, batchId);
			}
			if (batch.LeaseId == null)
			{
				throw new StructuredRpcException(StatusCode.PermissionDenied, "Batch {JobId}:{BatchId} has no lease id", job.Id, batchId);
			}

			ClaimsPrincipal principal = context.GetHttpContext().User;
			if (!principal.HasSessionClaim(batch.SessionId.Value) && !principal.HasLeaseClaim(batch.LeaseId.Value))
			{
				throw new StructuredRpcException(StatusCode.PermissionDenied, "Session id {SessionId} not valid for batch {JobId}:{BatchId}. Expected {ExpectedSessionId}.", principal.GetSessionClaim() ?? SessionId.Empty, job.Id, batchId, batch.SessionId.Value);
			}

			return batch;
		}

		/// <summary>
		/// Updates the state of a jobstep
		/// </summary>
		/// <param name="request">Request arguments</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the new agent</returns>
		public async Task<Empty> UpdateStepAsync(RpcUpdateStepRequest request, ServerCallContext context)
		{
			IJob? job = await _jobService.GetJobAsync(JobId.Parse(request.JobId));
			if (job == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Job {JobId} not found", request.JobId);
			}
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out StreamConfig? streamConfig))
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Stream {StreamId} not found", job.StreamId);
			}

			IJobStepBatch batch = AuthorizeBatch(job, JobStepBatchId.Parse(request.BatchId), context);

			JobStepId stepId = JobStepId.Parse(request.StepId);
			if (!batch.TryGetStep(stepId, out IJobStep? step))
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Unable to find step {JobId}:{BatchId}:{StepId}", job.Id, batch.Id, stepId);
			}

			JobStepError? error = null;
			if (step.Error == JobStepError.None && step.HasTimedOut(_clock.UtcNow))
			{
				error = JobStepError.TimedOut;
			}

			await _jobService.UpdateStepAsync(job, batch.Id, JobStepId.Parse(request.StepId), streamConfig, request.State, request.Outcome, error, null, null, null, null, null);
			return new Empty();
		}

		/// <summary>
		/// Get the state of a jobstep
		/// </summary>
		/// <param name="request">Request arguments</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the step</returns>
		public async Task<RpcGetStepResponse> GetStepAsync(GetStepRequest request, ServerCallContext context)
		{
			IJob job = await GetJobAsync(JobId.Parse(request.JobId), context.CancellationToken);
			IJobStepBatch batch = AuthorizeBatch(job, JobStepBatchId.Parse(request.BatchId), context);

			JobStepId stepId = JobStepId.Parse(request.StepId);
			if (!batch.TryGetStep(stepId, out IJobStep? step))
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Unable to find step {JobId}:{BatchId}:{StepId}", job.Id, batch.Id, stepId);
			}

			return new RpcGetStepResponse { Outcome = step.Outcome, State = step.State, AbortRequested = step.AbortRequested || step.HasTimedOut(_clock.UtcNow) };
		}

		/// <summary>
		/// Updates the state of a jobstep
		/// </summary>
		/// <param name="request">Request arguments</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the new agent</returns>
		public async Task<UpdateGraphResponse> UpdateGraphAsync(UpdateGraphRequest request, ServerCallContext context)
		{
			JobId jobIdValue = JobId.Parse(request.JobId);
			for (; ; )
			{
				IJob? job = await _jobService.GetJobAsync(jobIdValue);
				if (job == null)
				{
					throw new StructuredRpcException(StatusCode.NotFound, "Resource not found");
				}
				if (!JobService.AuthorizeSession(job, context.GetHttpContext().User))
				{
					throw new StructuredRpcException(StatusCode.PermissionDenied, "Access denied");
				}

				// Get the graph state
				IGraph oldGraph = await _graphs.GetAsync(job.GraphHash);

				// Add all the new nodes
				List<NewGroup> newGroups = new List<NewGroup> { new NewGroup(oldGraph, oldGraph.Groups[0]) };
				foreach (CreateGroupRequest group in request.Groups)
				{
					List<NewNode> newNodes = new List<NewNode>();
					foreach (CreateNodeRequest node in group.Nodes)
					{
						NewNode newNode = new NewNode(node.Name, node.Inputs.ToList(), node.Outputs.ToList(), node.InputDependencies.ToList(), node.OrderDependencies.ToList(), node.Priority, node.AllowRetry, node.RunEarly, node.Warnings, new Dictionary<string, string>(node.Credentials), new Dictionary<string, string>(node.Properties), new NodeAnnotations(node.Annotations));
						newNodes.Add(newNode);
					}
					newGroups.Add(new NewGroup(group.AgentType, newNodes));
				}

				// Add all the new aggregates
				List<NewAggregate> newAggregates = new List<NewAggregate>();
				foreach (CreateAggregateRequest aggregate in request.Aggregates)
				{
					NewAggregate newAggregate = new NewAggregate(aggregate.Name, aggregate.Nodes.ToList());
					newAggregates.Add(newAggregate);
				}

				// Add all the new labels
				List<NewLabel> newLabels = new List<NewLabel>();
				foreach (CreateLabelRequest label in request.Labels)
				{
					NewLabel newLabel = new NewLabel();
					newLabel.DashboardName = String.IsNullOrEmpty(label.DashboardName) ? null : label.DashboardName;
					newLabel.DashboardCategory = String.IsNullOrEmpty(label.DashboardCategory) ? null : label.DashboardCategory;
					newLabel.UgsName = String.IsNullOrEmpty(label.UgsName) ? null : label.UgsName;
					newLabel.UgsProject = String.IsNullOrEmpty(label.UgsProject) ? null : label.UgsProject;
					newLabel.Change = label.Change;
					newLabel.RequiredNodes = label.RequiredNodes.ToList();
					newLabel.IncludedNodes = label.IncludedNodes.ToList();
					newLabels.Add(newLabel);
				}

				// Add all the new artifacts
				List<NewGraphArtifact> newArtifacts = new List<NewGraphArtifact>();
				foreach (CreateGraphArtifactRequest artifact in request.Artifacts)
				{
					ArtifactName name = new ArtifactName(StringId.Sanitize(artifact.Name));
					ArtifactType type = new ArtifactType(StringId.Sanitize(artifact.Type));

					string description = artifact.Description;
					if (String.IsNullOrEmpty(description))
					{
						description = artifact.Name;
					}

					newArtifacts.Add(new NewGraphArtifact(name, type, description, artifact.BasePath, artifact.Keys.ToList(), artifact.Metadata.ToList(), artifact.OutputName));
				}

				// Create the new graph
				IGraph newGraph = await _graphs.AppendAsync(null, newGroups, newAggregates, newLabels, newArtifacts);

				// Try to update the graph with the new value
				IJob? newJob = await _jobService.TryUpdateGraphAsync(job, oldGraph, newGraph, context.CancellationToken);
				if (newJob != null)
				{
					_logger.LogInformation("Updating graph for {JobId} from {OldGraphHash} to {NewGraphHash}", job.Id, oldGraph.Id, newJob.GraphHash);
					return new UpdateGraphResponse();
				}
			}
		}

		/// <summary>
		/// Creates a set of events
		/// </summary>
		/// <param name="request">Request arguments</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the new agent</returns>
		public async Task<Empty> CreateEventsAsync(CreateEventsRequest request, ServerCallContext context)
		{
			if (!_globalConfig.Value.Authorize(LogAclAction.CreateEvent, context.GetHttpContext().User))
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Access denied");
			}

			List<NewLogEventData> newEvents = new List<NewLogEventData>();
			foreach (CreateEventRequest createEvent in request.Events)
			{
				NewLogEventData newEvent = new NewLogEventData();
				newEvent.LogId = LogId.Parse(createEvent.LogId);
				newEvent.Severity = createEvent.Severity;
				newEvent.LineIndex = createEvent.LineIndex;
				newEvent.LineCount = createEvent.LineCount;
				newEvents.Add(newEvent);
			}
			await _logFileService.CreateEventsAsync(newEvents, context.CancellationToken);
			return new Empty();
		}

		/// <summary>
		/// Uploads a new artifact
		/// </summary>
		/// <param name="reader">Request arguments</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the new agent</returns>
		public async Task<UploadArtifactResponse> UploadArtifactAsync(IAsyncStreamReader<UploadArtifactRequest> reader, ServerCallContext context)
		{
			// Advance to the metadata object
			if (!await reader.MoveNext())
			{
				throw new StructuredRpcException(StatusCode.DataLoss, "Missing request for artifact upload");
			}

			// Read the request object
			UploadArtifactMetadata? metadata = reader.Current.Metadata;
			if (metadata == null)
			{
				throw new StructuredRpcException(StatusCode.DataLoss, "Expected metadata in first artifact request");
			}

			// Get the job and step
			IJob job = await GetJobAsync(JobId.Parse(metadata.JobId), context.CancellationToken);
			AuthorizeBatch(job, JobStepBatchId.Parse(metadata.BatchId), context);

			IJobStep? step;
			if (!job.TryGetStep(JobStepBatchId.Parse(metadata.BatchId), JobStepId.Parse(metadata.StepId), out step))
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Unable to find step {JobId}:{BatchId}:{StepId}", job.Id, metadata.BatchId, metadata.StepId);
			}

			// Upload the stream
			using (ArtifactChunkStream inputStream = new ArtifactChunkStream(reader, metadata.Length))
			{
				IArtifactV1 artifact = await _artifactCollection.CreateArtifactAsync(job.Id, step.Id, metadata.Name, metadata.MimeType, inputStream, context.CancellationToken);

				UploadArtifactResponse response = new UploadArtifactResponse();
				response.Id = artifact.Id.ToString();
				return response;
			}
		}

		/// <summary>
		/// Uploads new test data
		/// </summary>
		/// <param name="reader">Request arguments</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the new agent</returns>
		public async Task<UploadTestDataResponse> UploadTestDataAsync(IAsyncStreamReader<UploadTestDataRequest> reader, ServerCallContext context)
		{
			IJob? job = null;
			IJobStep? jobStep = null;

			List<(string key, BsonDocument document)> data = new List<(string key, BsonDocument document)>();

			while (await reader.MoveNext())
			{
				UploadTestDataRequest request = reader.Current;

				JobId jobId = JobId.Parse(request.JobId);
				if (job == null)
				{
					job = await _jobService.GetJobAsync(jobId, context.CancellationToken);
					if (job == null)
					{
						throw new StructuredRpcException(StatusCode.NotFound, "Unable to find job {JobId}", jobId);
					}
				}
				else if (jobId != job.Id)
				{
					throw new StructuredRpcException(StatusCode.InvalidArgument, "Job {JobId} does not match previous Job {JobId} in request", jobId, job.Id);
				}

				JobStepId jobStepId = JobStepId.Parse(request.JobStepId);

				if (jobStep == null)
				{
					if (!job.TryGetStep(jobStepId, out jobStep))
					{
						throw new StructuredRpcException(StatusCode.NotFound, "Unable to find step {JobStepId} on job {JobId}", jobStepId, jobId);
					}
				}
				else if (jobStep.Id != jobStepId)
				{
					throw new StructuredRpcException(StatusCode.InvalidArgument, "Job step {JobStepId} does not match previous Job step {JobStepId} in request", jobStepId, jobStep.Id);
				}

				string text = Encoding.UTF8.GetString(request.Value.ToArray());
				BsonDocument document = BsonSerializer.Deserialize<BsonDocument>(text);
				data.Add((request.Key, document));
			}

			if (job != null && jobStep != null)
			{
				await _testData.AddAsync(job, jobStep, data.ToArray(), context.CancellationToken);
			}
			else
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Unable to get job or step for test data upload");
			}

			return new UploadTestDataResponse();
		}

		/// <summary>
		/// Create a new report on a job or job step
		/// </summary>
		/// <param name="request"></param>
		/// <param name="context"></param>
		/// <returns></returns>
		public async Task<CreateReportResponse> CreateReportAsync(CreateReportRequest request, ServerCallContext context)
		{
			IJob job = await GetJobAsync(JobId.Parse(request.JobId), context.CancellationToken);
			if (!_globalConfig.Value.TryGetStream(job.StreamId, out StreamConfig? streamConfig))
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Stream {StreamId} not found", job.StreamId);
			}

			IJobStepBatch batch = AuthorizeBatch(job, JobStepBatchId.Parse(request.BatchId), context);

			Report newReport = new Report { Name = request.Name, Placement = request.Placement, Content = request.Content };
			if (request.Scope == ReportScope.Job)
			{
				_logger.LogDebug("Adding report to job {JobId}: {Name} -> {Content}", job.Id, request.Name, request.Content);
				await _jobService.UpdateJobAsync(job, reports: new List<Report> { newReport });
			}
			else
			{
				_logger.LogDebug("Adding report to step {JobId}:{BatchId}:{StepId}: {Name} -> {Content}", job.Id, batch.Id, request.StepId, request.Name, request.Content);
				await _jobService.UpdateStepAsync(job, batch.Id, JobStepId.Parse(request.StepId), streamConfig, newReports: new List<Report> { newReport });
			}

			return new CreateReportResponse();
		}
	}
}
