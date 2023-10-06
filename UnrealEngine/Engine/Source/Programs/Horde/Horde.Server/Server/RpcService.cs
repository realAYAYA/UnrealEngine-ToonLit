// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Security.Claims;
using System.Threading;
using System.Threading.Tasks;
using Amazon.EC2.Model;
using EpicGames.Core;
using Google.Protobuf;
using Google.Protobuf.WellKnownTypes;
using Grpc.Core;
using Horde.Server.Acls;
using Horde.Server.Agents;
using Horde.Server.Agents.Pools;
using Horde.Server.Agents.Sessions;
using Horde.Server.Agents.Software;
using Horde.Server.Jobs;
using Horde.Server.Tasks;
using Horde.Server.Telemetry;
using Horde.Server.Tools;
using Horde.Server.Utilities;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Messages.Telemetry;
using Microsoft.AspNetCore.Authorization;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Server.Server
{
	using RpcAgentCapabilities = HordeCommon.Rpc.Messages.AgentCapabilities;
	using RpcDeviceCapabilities = HordeCommon.Rpc.Messages.DeviceCapabilities;
	using RpcGetStreamResponse = HordeCommon.Rpc.GetStreamResponse;
	using RpcGetJobResponse = HordeCommon.Rpc.GetJobResponse;
	using RpcGetStepResponse = HordeCommon.Rpc.GetStepResponse;
	using RpcUpdateJobRequest = HordeCommon.Rpc.UpdateJobRequest;
	using RpcUpdateStepRequest = HordeCommon.Rpc.UpdateStepRequest;

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

		readonly AgentService _agentService;
		readonly PoolService _poolService;
		readonly LifetimeService _lifetimeService;
		readonly ITelemetrySink _telemetrySink;
		readonly ConformTaskSource _conformTaskSource;
		readonly JobRpcCommon _jobRpcCommon;
		readonly IToolCollection _toolCollection;
		readonly AclService _aclService;
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public RpcService(AgentService agentService, PoolService poolService, LifetimeService lifetimeService, ITelemetrySink telemetrySink, ConformTaskSource conformTaskSource, JobRpcCommon jobRpcCommon, IToolCollection toolCollection, AclService aclService, IOptionsSnapshot<GlobalConfig> globalConfig, ILogger<RpcService> logger)
		{
			_agentService = agentService;
			_poolService = poolService;
			_lifetimeService = lifetimeService;
			_telemetrySink = telemetrySink;
			_conformTaskSource = conformTaskSource;
			_jobRpcCommon = jobRpcCommon;
			_toolCollection = toolCollection;
			_aclService = aclService;
			_globalConfig = globalConfig;
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
				HashSet<AgentWorkspace> conformWorkspaces = await _poolService.GetWorkspacesAsync(agent, DateTime.UtcNow, _globalConfig.Value);
				bool pendingConform = !conformWorkspaces.SetEquals(newWorkspaces) || (agent.RequestFullConform && !request.RemoveUntrackedFiles);

				// Update the workspaces
				if (await _agentService.TryUpdateWorkspacesAsync(agent, newWorkspaces, pendingConform))
				{
					UpdateAgentWorkspacesResponse response = new UpdateAgentWorkspacesResponse();
					if (pendingConform)
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

			if (capabilities == null)
			{
				return;
			}
			properties.AddRange(capabilities.Properties);

			if (capabilities.Devices.Count <= 0)
			{
				return;
			}

			RpcDeviceCapabilities device = capabilities.Devices[0];
			if (device.Properties == null)
			{
				return;
			}

			properties.AddRange(device.Properties);
			CopyPropertyToResource(KnownPropertyNames.LogicalCores, properties, resources);
			CopyPropertyToResource(KnownPropertyNames.Ram, properties, resources);
		}

		/// <summary>
		/// Creates a new agent
		/// </summary>
		/// <param name="request">Request to create a new agent</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the new agent</returns>
		public override async Task<CreateAgentResponse> CreateAgent(CreateAgentRequest request, ServerCallContext context)
		{
			using IDisposable scope = _logger.BeginScope("CreateAgent({AgentId})", request.Name.ToString());

			if (!_globalConfig.Value.Authorize(AgentAclAction.CreateAgent, context.GetHttpContext().User))
			{
				throw new StructuredRpcException(StatusCode.PermissionDenied, "User is not authenticated to create new agents");
			}

			// TODO: Do not allow overwriting existing agents unless explicitly asked to. We may be reusing an IP or hostname, and we can't trust the agent is what it says it is.
			IAgent? agent = await _agentService.GetAgentAsync(new AgentId(request.Name));
			if (agent == null)
			{
				agent = await _agentService.CreateAgentAsync(request.Name, _globalConfig.Value.ServerSettings.EnableNewAgentsByDefault, null);
			}

			List<AclClaimConfig> claims = new List<AclClaimConfig>();
			claims.Add(new AclClaimConfig(HordeClaimTypes.Agent, agent.Id.ToString()));

			CreateAgentResponse response = new CreateAgentResponse();
			response.Id = agent.Id.ToString();
			response.Token = await _aclService.IssueBearerTokenAsync(claims, null);

			return response;
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

			AgentId agentId = new AgentId(request.Id);
			using IDisposable scope = _logger.BeginScope("CreateSession({AgentId})", agentId.ToString());

			GlobalConfig globalConfig = _globalConfig.Value;

			// Find the agent
			IAgent? agent = await _agentService.GetAgentAsync(agentId);
			if (agent == null)
			{
				if (!globalConfig.Authorize(AgentAclAction.CreateAgent, context.GetHttpContext().User))
				{
					throw new StructuredRpcException(StatusCode.PermissionDenied, "User is not authenticated to create new agents");
				}

				agent = await _agentService.CreateAgentAsync(request.Id, true, null);
			}

			// Make sure we're allowed to create sessions on this agent
			ClaimsPrincipal user = context.GetHttpContext().User;
			if (!globalConfig.Authorize(SessionAclAction.CreateSession, user) && !user.HasAgentClaim(agentId))
			{
				throw new StructuredRpcException(StatusCode.PermissionDenied, "User is not authenticated to create session for {AgentId}", agentId);
			}

			// Get the known properties for this agent
			GetCapabilities(request.Capabilities, out List<string> properties, out Dictionary<string, int> resources);

			// Create a new session
			agent = await _agentService.CreateSessionAsync(agent, request.Status, properties, resources, request.Version);
			if (agent == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Agent {AgentId} not found", agentId);
			}

			// Create the response
			CreateSessionResponse response = new CreateSessionResponse();
			response.AgentId = agent.Id.ToString();
			response.SessionId = agent.SessionId.ToString();
			response.ExpiryTime = Timestamp.FromDateTime(agent.SessionExpiresAt!.Value);
			response.Token = await _agentService.IssueSessionTokenAsync(agent.Id, agent.SessionId!.Value);
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
					return task.IsCanceled? false : task.Result;
				}, TaskScheduler.Current);

				// Get the current agent state
				IAgent? agent = await _agentService.GetAgentAsync(new AgentId(request.AgentId));
				if (agent != null)
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
				try
				{
					while (await nextRequestTask)
					{
						nextRequestTask = reader.MoveNext();
					}
				}
				catch (Exception ex)
				{
					_logger.LogDebug(ex, "Ignoring exception while finishing UpdateSession request.");
				}
			}
		}

		/// <inheritdoc/>
		public override Task<RpcGetStreamResponse> GetStream(GetStreamRequest request, ServerCallContext context) => _jobRpcCommon.GetStream(request, context);

		/// <inheritdoc/>
		public override Task<RpcGetJobResponse> GetJob(GetJobRequest request, ServerCallContext context) => _jobRpcCommon.GetJob(request, context);

		/// <inheritdoc/>
		public override Task<Empty> UpdateJob(RpcUpdateJobRequest request, ServerCallContext context) => _jobRpcCommon.UpdateJob(request, context);

		/// <inheritdoc/>
		public override Task<BeginBatchResponse> BeginBatch(BeginBatchRequest request, ServerCallContext context) => _jobRpcCommon.BeginBatch(request, context);

		/// <inheritdoc/>
		public override Task<Empty> FinishBatch(FinishBatchRequest request, ServerCallContext context) => _jobRpcCommon.FinishBatch(request, context);

		/// <inheritdoc/>
		public override Task<BeginStepResponse> BeginStep(BeginStepRequest request, ServerCallContext context) => _jobRpcCommon.BeginStep(request, context);

		/// <inheritdoc/>
		public override Task<Empty> UpdateStep(RpcUpdateStepRequest request, ServerCallContext context) => _jobRpcCommon.UpdateStep(request, context);

		/// <inheritdoc/>
		public override Task<RpcGetStepResponse> GetStep(GetStepRequest request, ServerCallContext context) => _jobRpcCommon.GetStep(request, context);

		/// <inheritdoc/>
		public override Task<UpdateGraphResponse> UpdateGraph(UpdateGraphRequest request, ServerCallContext context) => _jobRpcCommon.UpdateGraph(request, context);

		/// <inheritdoc/>
		public override Task<Empty> CreateEvents(CreateEventsRequest request, ServerCallContext context) => _jobRpcCommon.CreateEvents(request, context);

		/// <summary>
		/// Downloads a new agent archive
		/// </summary>
		/// <param name="request">Request arguments</param>
		/// <param name="responseStream">Writer for the output data</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>Information about the new agent</returns>
		public override async Task DownloadSoftware(DownloadSoftwareRequest request, IServerStreamWriter<DownloadSoftwareResponse> responseStream, ServerCallContext context)
		{
			int colonIdx = request.Version.IndexOf(':', StringComparison.Ordinal);
			ToolId toolId = new ToolId(request.Version.Substring(0, colonIdx));
			string version = request.Version.Substring(colonIdx + 1);

			ToolConfig? toolConfig;
			if (!_globalConfig.Value.TryGetTool(toolId, out toolConfig))
			{
				throw new StructuredRpcException(StatusCode.NotFound, $"Missing tool {toolId}");
			}
			if (!toolConfig.Authorize(ToolAclAction.DownloadTool, context.GetHttpContext().User))
			{
				throw new StructuredRpcException(StatusCode.NotFound, "Access to software is forbidden");
			}

			ITool? tool = await _toolCollection.GetAsync(toolId, _globalConfig.Value);
			if (tool == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, $"Missing tool {toolId}");
			}

			IToolDeployment? deployment = tool.Deployments.LastOrDefault(x => x.Version.Equals(version, StringComparison.Ordinal));
			if (deployment == null)
			{
				throw new StructuredRpcException(StatusCode.NotFound, $"Missing tool version {version}");
			}

			using Stream stream = await _toolCollection.GetDeploymentZipAsync(tool, deployment, context.CancellationToken);
			using (IMemoryOwner<byte> buffer = MemoryPool<byte>.Shared.Rent(128 * 1024))
			{
				for(; ;)
				{
					int read = await stream.ReadAsync(buffer.Memory, context.CancellationToken);
					if (read == 0)
					{
						break;
					}

					DownloadSoftwareResponse response = new DownloadSoftwareResponse();
					response.Data = UnsafeByteOperations.UnsafeWrap(buffer.Memory.Slice(0, read));
					await responseStream.WriteAsync(response);
				}
			}
		}

		/// <summary>
		/// Receives telemetry events from agents
		/// </summary>
		/// <param name="request">Request arguments</param>
		/// <param name="context">Context for the RPC call</param>
		/// <returns>An empty response</returns>
		public override Task<Empty> SendTelemetryEvents(SendTelemetryEventsRequest request, ServerCallContext context)
		{
			foreach (WrappedTelemetryEvent e in request.Events)
			{
				switch (e.EventCase)
				{
					case WrappedTelemetryEvent.EventOneofCase.AgentMetadata: _telemetrySink.SendEvent("Agent.Metadata", e.AgentMetadata); break;
					case WrappedTelemetryEvent.EventOneofCase.Cpu: _telemetrySink.SendEvent("Agent.Cpu", e.Cpu); break;
					case WrappedTelemetryEvent.EventOneofCase.Mem: _telemetrySink.SendEvent("Agent.Memory", e.Mem); break;
					default: _logger.LogError("Unhandled wrapped telemetry type {Type}", e.EventCase.ToString()); break;
				}
			}

			return Task.FromResult(new Empty());
		}

		/// <inheritdoc/>
		public override Task<Empty> WriteOutput(WriteOutputRequest request, ServerCallContext context) => _jobRpcCommon.WriteOutput(request, context);

		/// <inheritdoc/>
		public override Task<UploadArtifactResponse> UploadArtifact(IAsyncStreamReader<UploadArtifactRequest> reader, ServerCallContext context) => _jobRpcCommon.UploadArtifact(reader, context);

		/// <inheritdoc/>
		public override Task<UploadTestDataResponse> UploadTestData(IAsyncStreamReader<UploadTestDataRequest> reader, ServerCallContext context) => _jobRpcCommon.UploadTestData(reader, context);

		/// <inheritdoc/>
		public override Task<CreateReportResponse> CreateReport(CreateReportRequest request, ServerCallContext context) => _jobRpcCommon.CreateReport(request, context);
	}
}
