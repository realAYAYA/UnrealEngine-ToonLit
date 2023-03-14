// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Security.Claims;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Google.Protobuf.WellKnownTypes;
using Grpc.Core;
using Horde.Build.Acls;
using Horde.Build.Agents;
using Horde.Build.Agents.Pools;
using Horde.Build.Agents.Sessions;
using Horde.Build.Agents.Software;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Artifacts;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Jobs.Templates;
using Horde.Build.Jobs.TestData;
using Horde.Build.Logs;
using Horde.Build.Streams;
using Horde.Build.Tasks;
using Horde.Build.Utilities;
using HordeCommon;
using HordeCommon.Rpc;
using Microsoft.AspNetCore.Authorization;
using Microsoft.Extensions.Logging;
using MongoDB.Bson;
using MongoDB.Bson.Serialization;

namespace Horde.Build.Server
{
	using AgentSoftwareChannelName = StringId<AgentSoftwareChannels>;
	using IStream = Horde.Build.Streams.IStream;
	using JobId = ObjectId<IJob>;
	using LogId = ObjectId<ILogFile>;
	using RpcAgentCapabilities = HordeCommon.Rpc.Messages.AgentCapabilities;
	using RpcDeviceCapabilities = HordeCommon.Rpc.Messages.DeviceCapabilities;
	using RpcGetStreamResponse = HordeCommon.Rpc.GetStreamResponse;
	using RpcGetJobResponse = HordeCommon.Rpc.GetJobResponse;
	using RpcGetStepResponse = HordeCommon.Rpc.GetStepResponse;
	using RpcUpdateJobRequest = HordeCommon.Rpc.UpdateJobRequest;
	using RpcUpdateStepRequest = HordeCommon.Rpc.UpdateStepRequest;
	using SessionId = ObjectId<ISession>;
	using StreamId = StringId<IStream>;

	/// <summary>
	/// Implements the Horde gRPC service for bots updating their status and dequeing work
	/// </summary>
	[Authorize]
	public class RpcService : HordeRpc.HordeRpcBase
	{
		/// <summary>
		/// Timeout before closing a long-polling request (client will retry again) 
		/// </summary>
		internal TimeSpan _longPollTimeout = TimeSpan.FromMinutes(9);
		readonly AclService _aclService;
		readonly AgentService _agentService;
		readonly StreamService _streamService;
		readonly JobService _jobService;
		readonly AgentSoftwareService _agentSoftwareService;
		readonly IArtifactCollection _artifactCollection;
		readonly ILogFileService _logFileService;
		readonly PoolService _poolService;
		readonly LifetimeService _lifetimeService;
		readonly IGraphCollection _graphs;
		readonly ITestDataCollection _testData;
		readonly IJobStepRefCollection _jobStepRefCollection;
		readonly ITemplateCollection _templateCollection;
		readonly ConformTaskSource _conformTaskSource;
		readonly HttpClient _httpClient;
		readonly IClock _clock;
		readonly ILogger<RpcService> _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public RpcService(AclService aclService, AgentService agentService, StreamService streamService, JobService jobService, AgentSoftwareService agentSoftwareService, IArtifactCollection artifactCollection, ILogFileService logFileService, PoolService poolService, LifetimeService lifetimeService, IGraphCollection graphs, ITestDataCollection testData, IJobStepRefCollection jobStepRefCollection, ITemplateCollection templateCollection, ConformTaskSource conformTaskSource, HttpClient httpClient, IClock clock, ILogger<RpcService> logger)
		{
			_aclService = aclService;
			_agentService = agentService;
			_streamService = streamService;
			_jobService = jobService;
			_agentSoftwareService = agentSoftwareService;
			_artifactCollection = artifactCollection;
			_logFileService = logFileService;
			_poolService = poolService;
			_lifetimeService = lifetimeService;
			_graphs = graphs;
			_testData = testData;
			_jobStepRefCollection = jobStepRefCollection;
			_templateCollection = templateCollection;
			_conformTaskSource = conformTaskSource;
			_httpClient = httpClient;
			_clock = clock;
			_logger = logger;
		}

		/// <summary>
		/// Waits until the server is terminating
		/// </summary>
		/// <param name="reader">Request reader</param>
		/// <param name="writer">Response writer</param>
		/// <param name="context">Context for the call</param>
		/// <returns>Response object</returns>
		public override async Task QueryServerState(IAsyncStreamReader<QueryServerStateRequest> reader, IServerStreamWriter<QueryServerStateResponse> writer, ServerCallContext context)
		{
			if (await reader.MoveNext())
			{
				QueryServerStateRequest request = reader.Current;
				_logger.LogInformation("Start server query for client {Name}", request.Name);

				// Return the current response
				QueryServerStateResponse response = new QueryServerStateResponse();
				response.Name = Dns.GetHostName();
				await writer.WriteAsync(response);

				// Move to the next request from the client. This should always be the end of the stream, but will not occur until the client stops requesting responses.
				Task<bool> moveNextTask = reader.MoveNext();

				// Wait for the client to close the stream or a shutdown to start
				Task longPollDelay = Task.Delay(_longPollTimeout);
				Task waitTask = await Task.WhenAny(moveNextTask, _lifetimeService.StoppingTask, longPollDelay);

				if (waitTask == moveNextTask)
				{
					throw new Exception("Unexpected request to QueryServerState posted from client.");
				}
				else if (waitTask == _lifetimeService.StoppingTask)
				{
					_logger.LogInformation("Notifying client {Name} of server shutdown", request.Name);
					await writer.WriteAsync(response);
				}
				else if (waitTask == longPollDelay)
				{
					// Send same response as server shutdown. In the agent perspective, they will be identical.
					await writer.WriteAsync(response);
				}
			}
		}

		/// <summary>
		/// Waits until the server is terminating
		/// </summary>
		/// <param name="reader">Request reader</param>
		/// <param name="writer">Response writer</param>
		/// <param name="context">Context for the call</param>
		/// <returns>Response object</returns>
		public override async Task QueryServerStateV2(IAsyncStreamReader<QueryServerStateRequest> reader, IServerStreamWriter<QueryServerStateResponse> writer, ServerCallContext context)
		{
			if (await reader.MoveNext())
			{
				QueryServerStateRequest request = reader.Current;
				_logger.LogDebug("Start server query for client {Name}", request.Name);

				try
				{
					// Return the current response
					QueryServerStateResponse response = new QueryServerStateResponse();
					response.Name = Dns.GetHostName();
					response.Stopping = _lifetimeService.IsStopping;
					await writer.WriteAsync(response);

					// Move to the next request from the client. This should always be the end of the stream, but will not occur until the client stops requesting responses.
					Task<bool> moveNextTask = reader.MoveNext();

					// Wait for the client to close the stream or a shutdown to start
					if (await Task.WhenAny(moveNextTask, _lifetimeService.StoppingTask) == _lifetimeService.StoppingTask)
					{
						response.Stopping = true;
						await writer.WriteAsync(response);
					}

					// Wait until the client has finished sending
					while (await moveNextTask)
					{
						moveNextTask = reader.MoveNext();
					}
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Exception in QueryServerState for {Name}", request.Name);
					throw;
				}
			}
		}

		/// <summary>
		/// Updates the workspaces synced for an agent
		/// </summary>
		/// <param name="request">The request parameters</param>
		/// <param name="context">Context for the call</param>
		/// <returns>Response object</returns>
		public override async Task<UpdateAgentWorkspacesResponse> UpdateAgentWorkspaces(UpdateAgentWorkspacesRequest request, ServerCallContext context)
		{
			for (; ; )
			{
				// Get the current agent state
				IAgent? agent = await _agentService.GetAgentAsync(new AgentId(request.AgentId));
				if (agent == null)
				{
					throw new StructuredRpcException(StatusCode.OutOfRange, "Agent {AgentId} does not exist", request.AgentId);
				}

				// Get the new workspaces
				List<AgentWorkspace> newWorkspaces = request.Workspaces.Select(x => new AgentWorkspace(x)).ToList();

				// Get the set of workspaces that are currently required
				HashSet<AgentWorkspace> conformWorkspaces = await _poolService.GetWorkspacesAsync(agent, DateTime.UtcNow);
				bool bPendingConform = !conformWorkspaces.SetEquals(newWorkspaces) || (agent.RequestFullConform && !request.RemoveUntrackedFiles);

				// Update the workspaces
				if (await _agentService.TryUpdateWorkspacesAsync(agent, newWorkspaces, bPendingConform))
				{
					UpdateAgentWorkspacesResponse response = new UpdateAgentWorkspacesResponse();
					if (bPendingConform)
					{
						response.Retry = await _conformTaskSource.GetWorkspacesAsync(agent, response.PendingWorkspaces);
						response.RemoveUntrackedFiles = request.RemoveUntrackedFiles || agent.RequestFullConform;
					}
					return response;
				}
			}
		}

		static void CopyPropertyToResource(string name, List<string> properties, Dictionary<string, int> resources)
		{
			foreach (string property in properties)
			{
				if (property.Length > name.Length && property.StartsWith(name, StringComparison.OrdinalIgnoreCase) && property[name.Length] == '=')
				{
					int value;
					if (Int32.TryParse(property.AsSpan(name.Length + 1), out value))
					{
						resources[name] = value;
					}
				}
			}
		}

		static void GetCapabilities(RpcAgentCapabilities? capabilities, out List<string> properties, out Dictionary<string, int> resources)
		{
			properties = new List<string>();
			resources = new Dictionary<string, int>();

			if (capabilities == null) return;
			properties.AddRange(capabilities.Properties);

			if (capabilities.Devices.Count <= 0) return;
			RpcDeviceCapabilities device = capabilities.Devices[0];
			if (device.Properties == null) return;

			properties.AddRange(device.Properties);
			CopyPropertyToResource(KnownPropertyNames.LogicalCores, properties, resources);
			CopyPropertyToResource(KnownPropertyNames.Ram, properties, resources);
		}

		/// <summary>
		/// Creates a new session
		/// </summary>
		/// <param name="request">Request to create a new agent</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the new agent</returns>
		public override async Task<CreateSessionResponse> CreateSession(CreateSessionRequest request, ServerCallContext context)
		{
			if (request.Capabilities == null)
			{
				throw new StructuredRpcException(StatusCode.InvalidArgument, "Capabilities may not be null");
			}

			AgentId agentId = new AgentId(request.Name);
			using IDisposable scope = _logger.BeginScope("CreateSession({AgentId})", agentId.ToString());

			// Find the agent
			IAgent? agent = await _agentService.GetAgentAsync(agentId);
			if (agent == null)
			{
				if (!await _aclService.AuthorizeAsync(AclAction.CreateAgent, context.GetHttpContext().User))
				{
					throw new StructuredRpcException(StatusCode.PermissionDenied, "User is not authenticated to create new agents");
				}

				agent = await _agentService.CreateAgentAsync(request.Name, true, null, null);
			}

			// Make sure we're allowed to create sessions on this agent
			if (!await _agentService.AuthorizeAsync(agent, AclAction.CreateSession, context.GetHttpContext().User, null))
			{
				throw new StructuredRpcException(StatusCode.PermissionDenied, "User is not authenticated to create session for {AgentId}", request.Name);
			}

			// Get the known properties for this agent
			GetCapabilities(request.Capabilities, out List<string> properties, out Dictionary<string, int> resources);

			// Create a new session
			agent = await _agentService.CreateSessionAsync(agent, request.Status, properties, resources, request.Version);
			if (agent == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Agent {AgentId} not found", request.Name);
			}

			// Create the response
			CreateSessionResponse response = new CreateSessionResponse();
			response.AgentId = agent.Id.ToString();
			response.SessionId = agent.SessionId.ToString();
			response.ExpiryTime = Timestamp.FromDateTime(agent.SessionExpiresAt!.Value);
			response.Token = _agentService.IssueSessionToken(agent.Id, agent.SessionId!.Value);
			return response;
		}

		/// <summary>
		/// Updates an agent session
		/// </summary>
		/// <param name="reader">Request to create a new agent</param>
		/// <param name="writer">Writer for response objects</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the new agent</returns>
		public override async Task UpdateSession(IAsyncStreamReader<UpdateSessionRequest> reader, IServerStreamWriter<UpdateSessionResponse> writer, ServerCallContext context)
		{
			// Read the request object
			Task<bool> nextRequestTask = reader.MoveNext();
			if (await nextRequestTask)
			{
				UpdateSessionRequest request = reader.Current;
				using IDisposable scope = _logger.BeginScope("UpdateSession for agent {AgentId}, session {SessionId}", request.AgentId, request.SessionId);

				_logger.LogDebug("Updating session for {AgentId}", request.AgentId);
				foreach (HordeCommon.Rpc.Messages.Lease lease in request.Leases)
				{
					_logger.LogDebug("Session {SessionId}, Lease {LeaseId} - State: {LeaseState}, Outcome: {LeaseOutcome}", request.SessionId, lease.Id, lease.State, lease.Outcome);
				}

				// Get a task for moving to the next item. This will only complete once the call has closed.
				using CancellationTokenSource cancellationSource = CancellationTokenSource.CreateLinkedTokenSource(context.CancellationToken);
				nextRequestTask = reader.MoveNext();
				nextRequestTask = nextRequestTask.ContinueWith(task => 
				{ 
					cancellationSource.Cancel(); 
					return task.Result; 
				}, TaskScheduler.Current);

				// Get the current agent state
				IAgent? agent = await _agentService.GetAgentAsync(new AgentId(request.AgentId));
				if(agent != null)
				{
					// Check we're authorized to update it
					if (!_agentService.AuthorizeSession(agent, context.GetHttpContext().User))
					{
						throw new StructuredRpcException(StatusCode.PermissionDenied, "Not authenticated for {AgentId}", request.AgentId);
					}

					// Get the new capabilities of this agent
					List<string>? properties = null;
					Dictionary<string, int>? resources = null;
					if (request.Capabilities != null)
					{
						GetCapabilities(request.Capabilities, out properties, out resources);
					}

					// Update the session
					try
					{
						agent = await _agentService.UpdateSessionWithWaitAsync(agent, SessionId.Parse(request.SessionId), request.Status, properties, resources, request.Leases, cancellationSource.Token);
					}
					catch (Exception ex)
					{
						_logger.LogError(ex, "Swallowed exception while updating session for {AgentId}.", request.AgentId);
					}
				}

				// Handle the invalid agent case
				if (agent == null)
				{
					throw new StructuredRpcException(StatusCode.NotFound, "Invalid agent name '{AgentId}'", request.AgentId);
				}

				// Create the new session info
				if (!context.CancellationToken.IsCancellationRequested)
				{
					UpdateSessionResponse response = new UpdateSessionResponse();
					response.Leases.Add(agent.Leases.Select(x => x.ToRpcMessage()));
					response.ExpiryTime = (agent.SessionExpiresAt == null) ? new Timestamp() : Timestamp.FromDateTime(agent.SessionExpiresAt.Value);
					await writer.WriteAsync(response);
				}

				// Wait for the client to close the stream
				while (await nextRequestTask)
				{
					nextRequestTask = reader.MoveNext();
				}
			}
		}

		/// <summary>
		/// Gets information about a stream
		/// </summary>
		/// <param name="request">Request arguments</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the new agent</returns>
		public override async Task<RpcGetStreamResponse> GetStream(GetStreamRequest request, ServerCallContext context)
		{
			StreamId streamIdValue = new StreamId(request.StreamId);

			IStream? stream = await _streamService.GetStreamAsync(streamIdValue);
			if (stream == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Stream {StreamId} does not exist", request.StreamId);
			}
			if (!await _streamService.AuthorizeAsync(stream, AclAction.ViewStream, context.GetHttpContext().User, null))
			{
				throw new StructuredRpcException(StatusCode.PermissionDenied, "Not authenticated to access stream {StreamId}", request.StreamId);
			}

			return stream.ToRpcResponse();
		}

		/// <summary>
		/// Gets information about a job
		/// </summary>
		/// <param name="request">Request arguments</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the new agent</returns>
		public override async Task<RpcGetJobResponse> GetJob(GetJobRequest request, ServerCallContext context)
		{
			JobId jobIdValue = new JobId(request.JobId.ToObjectId());

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
		public override async Task<Empty> UpdateJob(RpcUpdateJobRequest request, ServerCallContext context)
		{
			JobId jobIdValue = new JobId(request.JobId);

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
		public override async Task<BeginBatchResponse> BeginBatch(BeginBatchRequest request, ServerCallContext context)
		{
			SubResourceId batchId = request.BatchId.ToSubResourceId();

			IJob? job = await _jobService.GetJobAsync(new JobId(request.JobId));
			if (job == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Job {JobId} not found", request.JobId);
			}

			IJobStepBatch batch = AuthorizeBatch(job, request.BatchId.ToSubResourceId(), context);
			job = await _jobService.UpdateBatchAsync(job, batchId, null, JobStepBatchState.Starting);

			if (job == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Batch {JobId}:{BatchId} not found for updating", request.JobId, request.BatchId);
			}

			IGraph graph = await _jobService.GetGraphAsync(job);

			BeginBatchResponse response = new BeginBatchResponse();
			response.LogId = batch.LogId.ToString();
			response.AgentType = graph.Groups[batch.GroupIdx].AgentType;
			return response;
		}

		/// <summary>
		/// Finishes executing a batch
		/// </summary>
		/// <param name="request">Request arguments</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the new agent</returns>
		public override async Task<Empty> FinishBatch(FinishBatchRequest request, ServerCallContext context)
		{
			IJob? job = await _jobService.GetJobAsync(new JobId(request.JobId));
			if (job == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Job {JobId} not found", request.JobId);
			}

			IJobStepBatch batch = AuthorizeBatch(job, request.BatchId.ToSubResourceId(), context);
			await _jobService.UpdateBatchAsync(job, batch.Id, null, JobStepBatchState.Complete);
			return new Empty();
		}

		/// <summary>
		/// Starts executing a step
		/// </summary>
		/// <param name="request">Request arguments</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the new agent</returns>
		public override async Task<BeginStepResponse> BeginStep(BeginStepRequest request, ServerCallContext context)
		{
			Boxed<ILogFile?> log = new Boxed<ILogFile?>(null);
			for (; ; )
			{
				BeginStepResponse? response = await TryBeginStep(request, log, context);
				if (response != null)
				{
					return response;
				}
			}
		}

		async Task<BeginStepResponse?> TryBeginStep(BeginStepRequest request, Boxed<ILogFile?> log, ServerCallContext context)
		{
			// Check the job exists and we can access it
			IJob? job = await _jobService.GetJobAsync(new JobId(request.JobId));
			if (job == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Job {JobId} not found", request.JobId);
			}

			// Find the batch being executed
			IJobStepBatch batch = AuthorizeBatch(job, request.BatchId.ToSubResourceId(), context);
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
			if (log.Value == null)
			{
				log.Value = await _logFileService.CreateLogFileAsync(job.Id, batch.SessionId, LogType.Json);
			}

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
			IJob? newJob = await _jobService.TryUpdateStepAsync(job, batch.Id, step.Id, JobStepState.Running, JobStepOutcome.Unspecified, newLogId: log.Value.Id);
			if (newJob != null)
			{
				BeginStepResponse response = new BeginStepResponse();
				response.State = BeginStepResponse.Types.Result.Ready;
				response.LogId = log.Value.Id.ToString();
				response.StepId = step.Id.ToString();
				response.Name = node.Name;
				response.Credentials.Add(credentials);

				string templateName = "<unknown>";
				if (job.TemplateHash != null)
				{
					ITemplate? template = await _templateCollection.GetAsync(job.TemplateHash);
					templateName = template != null ? template.Name : templateName;
				}
				
				response.EnvVars.Add("UE_HORDE_TEMPLATEID", job.TemplateId.ToString());
				response.EnvVars.Add("UE_HORDE_TEMPLATENAME", templateName);
				response.EnvVars.Add("UE_HORDE_STEPNAME", node.Name);

				IJobStepRef? lastStep = await _jobStepRefCollection.GetPrevStepForNodeAsync(job.StreamId, job.TemplateId, node.Name, job.Change);
				if (lastStep != null)
				{
					response.EnvVars.Add("UE_HORDE_LAST_CL", lastStep.Change.ToString(CultureInfo.InvariantCulture));

					if (lastStep.Outcome == JobStepOutcome.Success)
					{
						response.EnvVars.Add("UE_HORDE_LAST_SUCCESS_CL", lastStep.Change.ToString(CultureInfo.InvariantCulture));
					}
					else if (lastStep.LastSuccess != null)
					{
						response.EnvVars.Add("UE_HORDE_LAST_SUCCESS_CL", lastStep.LastSuccess.Value.ToString(CultureInfo.InvariantCulture));
					}

					if (lastStep.Outcome == JobStepOutcome.Success || lastStep.Outcome == JobStepOutcome.Warnings)
					{
						response.EnvVars.Add("UE_HORDE_LAST_WARNING_CL", lastStep.Change.ToString(CultureInfo.InvariantCulture));
					}
					else if (lastStep.LastWarning != null)
					{
						response.EnvVars.Add("UE_HORDE_LAST_WARNING_CL", lastStep.LastWarning.Value.ToString(CultureInfo.InvariantCulture));
					}
				}

				IStream? stream = await _streamService.GetStreamAsync(job.StreamId);
				if (stream != null)
				{
					foreach (TokenConfig tokenConfig in stream.Config.Tokens)
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
				}

				if (node.Properties != null)
				{
					response.Properties.Add(node.Properties);
				}
				response.Warnings = node.Warnings;
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
					using Stream stream = await response.Content.ReadAsStreamAsync(cancellationToken);
					GetTokenResponse? token = await JsonSerializer.DeserializeAsync<GetTokenResponse>(stream, cancellationToken: cancellationToken);
					return token?.AccessToken;
				}
			}
		}

		async Task<IJob> GetJobAsync(JobId jobId)
		{
			IJob? job = await _jobService.GetJobAsync(jobId);
			if (job == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Job {JobId} not found", jobId);
			}
			return job;
		}

		static IJobStepBatch AuthorizeBatch(IJob job, SubResourceId batchId, ServerCallContext context)
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

			ClaimsPrincipal principal = context.GetHttpContext().User;
			if (!principal.HasSessionClaim(batch.SessionId.Value))
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
		public override async Task<Empty> UpdateStep(RpcUpdateStepRequest request, ServerCallContext context)
		{
			IJob? job = await _jobService.GetJobAsync(new JobId(request.JobId));
			if (job == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Job {JobId} not found", request.JobId);
			}

			IJobStepBatch batch = AuthorizeBatch(job, request.BatchId.ToSubResourceId(), context);

			SubResourceId stepId = request.StepId.ToSubResourceId();
			if (!batch.TryGetStep(stepId, out IJobStep? step))
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Unable to find step {JobId}:{BatchId}:{StepId}", job.Id, batch.Id, stepId);
			}

			JobStepError? error = null;
			if (step.Error == JobStepError.None && step.HasTimedOut(_clock.UtcNow))
			{
				error = JobStepError.TimedOut;
			}

			await _jobService.UpdateStepAsync(job, batch.Id, request.StepId.ToSubResourceId(), request.State, request.Outcome, error, null, null, null, null, null);
			return new Empty();
		}

		/// <summary>
		/// Get the state of a jobstep
		/// </summary>
		/// <param name="request">Request arguments</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the step</returns>
		public override async Task<RpcGetStepResponse> GetStep(GetStepRequest request, ServerCallContext context)
		{
			IJob job = await GetJobAsync(new JobId(request.JobId));
			IJobStepBatch batch = AuthorizeBatch(job, request.BatchId.ToSubResourceId(), context);

			SubResourceId stepId = request.StepId.ToSubResourceId();
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
		public override async Task<UpdateGraphResponse> UpdateGraph(UpdateGraphRequest request, ServerCallContext context)
		{
			List<NewGroup> newGroups = new List<NewGroup>();
			foreach (CreateGroupRequest group in request.Groups)
			{
				List<NewNode> newNodes = new List<NewNode>();
				foreach (CreateNodeRequest node in group.Nodes)
				{
					NewNode newNode = new NewNode(node.Name, node.InputDependencies.ToList(), node.OrderDependencies.ToList(), node.Priority, node.AllowRetry, node.RunEarly, node.Warnings, new Dictionary<string, string>(node.Credentials), new Dictionary<string, string>(node.Properties), new NodeAnnotations(node.Annotations));
					newNodes.Add(newNode);
				}
				newGroups.Add(new NewGroup(group.AgentType, newNodes));
			}

			List<NewAggregate> newAggregates = new List<NewAggregate>();
			foreach (CreateAggregateRequest aggregate in request.Aggregates)
			{
				NewAggregate newAggregate = new NewAggregate(aggregate.Name, aggregate.Nodes.ToList());
				newAggregates.Add(newAggregate);
			}

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

			JobId jobIdValue = new JobId(request.JobId);
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

				IGraph graph = await _jobService.GetGraphAsync(job);
				graph = await _graphs.AppendAsync(graph, newGroups, newAggregates, newLabels);

				IJob? newJob = await _jobService.TryUpdateGraphAsync(job, graph);
				if (newJob != null)
				{
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
		public override async Task<Empty> CreateEvents(CreateEventsRequest request, ServerCallContext context)
		{
			if (!await _aclService.AuthorizeAsync(AclAction.CreateEvent, context.GetHttpContext().User))
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Access denied");
			}

			List<NewLogEventData> newEvents = new List<NewLogEventData>();
			foreach (CreateEventRequest createEvent in request.Events)
			{
				NewLogEventData newEvent = new NewLogEventData();
				newEvent.LogId = new LogId(createEvent.LogId);
				newEvent.Severity = createEvent.Severity;
				newEvent.LineIndex = createEvent.LineIndex;
				newEvent.LineCount = createEvent.LineCount;
				newEvents.Add(newEvent);
			}
			await _logFileService.CreateEventsAsync(newEvents);
			return new Empty();
		}

		/// <summary>
		/// Writes output to a log file
		/// </summary>
		/// <param name="request">Request arguments</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the new agent</returns>
		public override async Task<Empty> WriteOutput(WriteOutputRequest request, ServerCallContext context)
		{
			ILogFile? logFile = await _logFileService.GetCachedLogFileAsync(new LogId(request.LogId));
			if (logFile == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Resource not found");
			}
			if (!LogFileService.AuthorizeForSession(logFile, context.GetHttpContext().User))
			{
				throw new StructuredRpcException(StatusCode.PermissionDenied, "Access denied");
			}

			await _logFileService.WriteLogDataAsync(logFile, request.Offset, request.LineIndex, request.Data.ToArray(), request.Flush);
			return new Empty();
		}

		/// <summary>
		/// Uploads a new agent archive
		/// </summary>
		/// <param name="request">Request arguments</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the new agent</returns>
		public override async Task<UploadSoftwareResponse> UploadSoftware(UploadSoftwareRequest request, ServerCallContext context)
		{
			if (!await _aclService.AuthorizeAsync(AclAction.UploadSoftware, context.GetHttpContext().User))
			{
				throw new StructuredRpcException(StatusCode.PermissionDenied, "Access to software is forbidden");
			}

			string version = await _agentSoftwareService.SetArchiveAsync(new AgentSoftwareChannelName(request.Channel), null, request.Data.ToArray());

			UploadSoftwareResponse response = new UploadSoftwareResponse();
			response.Version = version;
			return response;
		}

		/// <summary>
		/// Downloads a new agent archive
		/// </summary>
		/// <param name="request">Request arguments</param>
		/// <param name="responseStream">Writer for the output data</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the new agent</returns>
		public override async Task DownloadSoftware(DownloadSoftwareRequest request, IServerStreamWriter<DownloadSoftwareResponse> responseStream, ServerCallContext context)
		{
			if (!await _aclService.AuthorizeAsync(AclAction.DownloadSoftware, context.GetHttpContext().User))
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Access to software is forbidden");
			}

			byte[]? data = await _agentSoftwareService.GetArchiveAsync(request.Version);
			if (data == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Missing version {Version}");
			}

			for (int offset = 0; offset < data.Length;)
			{
				int nextOffset = Math.Min(offset + 128 * 1024, data.Length);

				DownloadSoftwareResponse response = new DownloadSoftwareResponse();
				response.Data = Google.Protobuf.ByteString.CopyFrom(data.AsSpan(offset, nextOffset - offset));

				await responseStream.WriteAsync(response);

				offset = nextOffset;
			}
		}

		/// <summary>
		/// Uploads a new artifact
		/// </summary>
		/// <param name="reader">Request arguments</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the new agent</returns>
		public override async Task<UploadArtifactResponse> UploadArtifact(IAsyncStreamReader<UploadArtifactRequest> reader, ServerCallContext context)
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
			IJob job = await GetJobAsync(new JobId(metadata.JobId));
			AuthorizeBatch(job, metadata.BatchId.ToSubResourceId(), context);

			IJobStep? step;
			if (!job.TryGetStep(metadata.BatchId.ToSubResourceId(), metadata.StepId.ToSubResourceId(), out step))
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Unable to find step {JobId}:{BatchId}:{StepId}", job.Id, metadata.BatchId, metadata.StepId);
			}

			// Upload the stream
			using (ArtifactChunkStream inputStream = new ArtifactChunkStream(reader, metadata.Length))
			{
				IArtifact artifact = await _artifactCollection.CreateArtifactAsync(job.Id, step.Id, metadata.Name, metadata.MimeType, inputStream);

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
		public override async Task<UploadTestDataResponse> UploadTestData(IAsyncStreamReader<UploadTestDataRequest> reader, ServerCallContext context)
		{
			IJob? job = null;
			IJobStep? jobStep = null;

			while (await reader.MoveNext())
			{
				UploadTestDataRequest request = reader.Current;

				JobId jobId = new JobId(request.JobId);
				if (job == null || jobId != job.Id)
				{
					job = await _jobService.GetJobAsync(jobId);
					if (job == null)
					{
						throw new StructuredRpcException(StatusCode.NotFound, "Unable to find job {JobId}", jobId);
					}
					jobStep = null;
				}

				SubResourceId jobStepId = request.JobStepId.ToSubResourceId();
				if (jobStep == null || jobStepId != jobStep.Id)
				{
					if (!job.TryGetStep(jobStepId, out jobStep))
					{
						throw new StructuredRpcException(StatusCode.NotFound, "Unable to find step {JobStepId} on job {JobId}", jobStepId, jobId);
					}
				}

				string text = Encoding.UTF8.GetString(request.Value.ToArray());
				BsonDocument document = BsonSerializer.Deserialize<BsonDocument>(text);
				await _testData.AddAsync(job, jobStep, request.Key, document);
			}

			return new UploadTestDataResponse();
		}

		/// <summary>
		/// Create a new report on a job or job step
		/// </summary>
		/// <param name="request"></param>
		/// <param name="context"></param>
		/// <returns></returns>
		public override async Task<CreateReportResponse> CreateReport(CreateReportRequest request, ServerCallContext context)
		{
			IJob job = await GetJobAsync(new JobId(request.JobId));
			IJobStepBatch batch = AuthorizeBatch(job, request.BatchId.ToSubResourceId(), context);

			Report newReport = new Report { Name = request.Name, Placement = request.Placement, ArtifactId = request.ArtifactId.ToObjectId() };
			if (request.Scope == ReportScope.Job)
			{
				_logger.LogDebug("Adding report to job {JobId}: {Name} -> {ArtifactId}", job.Id, request.Name, request.ArtifactId);
				await _jobService.UpdateJobAsync(job, reports: new List<Report> { newReport });
			}
			else
			{
				_logger.LogDebug("Adding report to step {JobId}:{BatchId}:{StepId}: {Name} -> {ArtifactId}", job.Id, batch.Id, request.StepId, request.Name, request.ArtifactId);
				await _jobService.UpdateStepAsync(job, batch.Id, request.StepId.ToSubResourceId(), newReports: new List<Report> { newReport });
			}

			return new CreateReportResponse();
		}
	}
}
