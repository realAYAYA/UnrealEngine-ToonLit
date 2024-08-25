// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Pools;
using EpicGames.Horde.Agents.Sessions;
using EpicGames.Horde.Common;
using Horde.Server.Agents.Leases;
using Horde.Server.Agents.Sessions;
using Horde.Server.Auditing;
using Horde.Server.Server;
using Horde.Server.Users;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Server.Agents
{
	/// <summary>
	/// Controller for the /api/v1/agents endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class AgentsController : HordeControllerBase
	{
		readonly AgentService _agentService;
		readonly IUserCollection _userCollection;
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;
		readonly ILogger<AgentsController> _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public AgentsController(AgentService agentService, IUserCollection userCollection, IOptionsSnapshot<GlobalConfig> globalConfig, ILogger<AgentsController> logger)
		{
			_agentService = agentService;
			_userCollection = userCollection;
			_globalConfig = globalConfig;
			_logger = logger;
		}

		/// <summary>
		/// Finds the agents matching specified criteria.
		/// </summary>
		/// <param name="poolId">The pool containing the agent</param>
		/// <param name="condition">Arbitrary condition to evaluate against the agents</param>
		/// <param name="includeDeleted">Whether to include agents marked as deleted</param>
		/// <param name="index">First result to return</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="modifiedAfter">If set, only returns agents modified after this time</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>List of matching agents</returns>
		[HttpGet]
		[Route("/api/v1/agents")]
		[ProducesResponseType(typeof(List<GetAgentResponse>), 200)]
		public async Task<ActionResult<List<object>>> FindAgentsAsync([FromQuery] PoolId? poolId = null, [FromQuery] Condition? condition = null, [FromQuery] bool includeDeleted = false, [FromQuery] int? index = null, [FromQuery] int? count = null, [FromQuery] DateTimeOffset? modifiedAfter = null, [FromQuery] PropertyFilter? filter = null)
		{
			if (!_globalConfig.Value.Authorize(AgentAclAction.ListAgents, User))
			{
				return Forbid(AgentAclAction.ListAgents);
			}

			IReadOnlyList<IAgent> agents = await _agentService.FindAgentsAsync(poolId, modifiedAfter?.UtcDateTime, null, includeDeleted, index, count, HttpContext.RequestAborted);

			List<object> responses = new List<object>();
			foreach (IAgent agent in agents)
			{
				if (condition == null || agent.SatisfiesCondition(condition))
				{
					responses.Add(await GetAgentResponseAsync(agent, filter));
				}
			}

			return responses;
		}

		/// <summary>
		/// Retrieve information about a specific agent
		/// </summary>
		/// <param name="agentId">Id of the agent to get information about</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>Information about the requested agent</returns>
		[HttpGet]
		[Route("/api/v1/agents/{agentId}")]
		[ProducesResponseType(typeof(GetAgentResponse), 200)]
		public async Task<ActionResult<object>> GetAgentAsync(AgentId agentId, [FromQuery] PropertyFilter? filter = null)
		{
			if (!_globalConfig.Value.Authorize(AgentAclAction.ViewAgent, User))
			{
				return Forbid(AgentAclAction.ViewAgent, agentId);
			}

			IAgent? agent = await _agentService.GetAgentAsync(agentId);
			if (agent == null)
			{
				return NotFound(agentId);
			}

			return await GetAgentResponseAsync(agent, filter);
		}

		/// <summary>
		/// Gets an individual agent response
		/// </summary>
		async ValueTask<object> GetAgentResponseAsync(IAgent agent, PropertyFilter? filter = null, CancellationToken cancellationToken = default)
		{
			double? rate = null;
			if (_globalConfig.Value.Authorize(ServerAclAction.ViewCosts, User))
			{
				rate = await _agentService.GetRateAsync(agent.Id, cancellationToken);
			}

			List<GetAgentLeaseResponse> leases = new List<GetAgentLeaseResponse>();
			foreach (AgentLease lease in agent.Leases)
			{
				try
				{
					Dictionary<string, string>? details = await _agentService.GetPayloadDetailsAsync(lease.Payload, cancellationToken);
					leases.Add(CreateGetAgentLeaseResponse(lease, details));
				}
				catch (Exception e)
				{
					_logger.LogError(e, "Failed getting payload details for agent lease {LeaseId}", lease.Id.ToString());
				}
			}

			return CreateGetAgentResponse(agent, leases, rate).ApplyFilter(filter);
		}

		internal static GetAgentLeaseResponse CreateGetAgentLeaseResponse(AgentLease lease, Dictionary<string, string>? details)
		{
			return new GetAgentLeaseResponse(lease.Id, lease.ParentId, null, null, lease.Name, lease.LogId, lease.StartTime, lease.ExpiryTime, lease.Active, details, null, lease.State);
		}

		internal static GetAgentLeaseResponse CreateGetAgentLeaseResponse(ILease lease, Dictionary<string, string>? details, double? agentRate)
		{
			return new GetAgentLeaseResponse(lease.Id, lease.ParentId, lease.AgentId, agentRate, lease.Name, lease.LogId, lease.StartTime, lease.FinishTime, lease.FinishTime == null, details, lease.Outcome, null);
		}

		static GetAgentResponse CreateGetAgentResponse(IAgent agent, List<GetAgentLeaseResponse> leases, double? rate)
		{
			return new GetAgentResponse(
				agent.Id,
				agent.Id.ToString(),
				agent.Enabled,
				agent.Status,
				rate,
				agent.SessionId,
				agent.Ephemeral,
				agent.IsSessionValid(DateTime.UtcNow),
				agent.Deleted,
				agent.RequestConform,
				agent.RequestFullConform,
				agent.RequestRestart,
				agent.RequestShutdown,
				agent.LastShutdownReason ?? "Unknown",
				agent.LastConformTime,
				agent.ConformAttemptCount,
				agent.LastConformTime,
				agent.Version?.ToString() ?? "Unknown",
				new List<string>(agent.Properties),
				new Dictionary<string, int>(agent.Resources),
				agent.UpdateTime,
				agent.LastStatusChange,
				agent.GetPools().Select(x => x.ToString()).ToList(),
				new { Devices = new[] { new { agent.Properties, agent.Resources } } },
				leases,
				agent.Workspaces.ConvertAll(x => CreateGetAgentWorkspaceResponse(x)),
				agent.Comment);
		}

		internal static GetAgentWorkspaceResponse CreateGetAgentWorkspaceResponse(AgentWorkspaceInfo workspace)
		{
			return new GetAgentWorkspaceResponse(workspace.Cluster, workspace.UserName, workspace.Identifier, workspace.Stream, workspace.View, workspace.Incremental, workspace.Method);
		}

		/// <summary>
		/// Update an agent's properties.
		/// </summary>
		/// <param name="agentId">Id of the agent to update.</param>
		/// <param name="update">Properties on the agent to update.</param>
		/// <returns>Http result code</returns>
		[HttpPut]
		[Route("/api/v1/agents/{agentId}")]
		public async Task<ActionResult> UpdateAgentAsync(AgentId agentId, [FromBody] UpdateAgentRequest update)
		{
			if (!_globalConfig.Value.Authorize(AgentAclAction.UpdateAgent, User))
			{
				return Forbid(AgentAclAction.UpdateAgent, agentId);
			}

			List<PoolId>? updatePools = update.Pools?.ConvertAll(x => new PoolId(x));

			IUser? user = await _userCollection.GetUserAsync(User, HttpContext.RequestAborted);
			string userName = user?.Name ?? "Unknown";

			for (; ; )
			{
				IAgent? agent = await _agentService.GetAgentAsync(agentId);
				if (agent == null)
				{
					return NotFound(agentId);
				}

				IAgent? newAgent = await _agentService.Agents.TryUpdateSettingsAsync(agent, update.Enabled, update.RequestConform, update.RequestFullConform, update.RequestRestart, update.RequestShutdown, update.RequestForceRestart, $"Manual ({userName})", update.Pools?.ConvertAll(x => new PoolId(x)), update.Comment);
				if (newAgent == null)
				{
					continue;
				}

				IAuditLogChannel<AgentId> logger = _agentService.Agents.GetLogger(agent.Id);
				if (agent.Enabled != newAgent.Enabled)
				{
					logger.LogInformation("Setting changed: Enabled = {State} ({UserName})", newAgent.Enabled, userName);
				}
				if (agent.RequestConform != newAgent.RequestConform)
				{
					logger.LogInformation("Setting changed: RequestConform = {State} ({UserName})", newAgent.RequestConform, userName);
				}
				if (agent.RequestFullConform != newAgent.RequestFullConform)
				{
					logger.LogInformation("Setting changed: RequestFullConform = {State} ({UserName})", newAgent.RequestFullConform, userName);
				}
				if (agent.RequestRestart != newAgent.RequestRestart)
				{
					logger.LogInformation("Setting changed: RequestRestart = {State} ({UserName})", newAgent.RequestRestart, userName);
				}
				if (agent.RequestShutdown != newAgent.RequestShutdown)
				{
					logger.LogInformation("Setting changed: RequestShutdown = {State} ({UserName})", newAgent.RequestShutdown, userName);
				}
				if (agent.Comment != newAgent.Comment)
				{
					logger.LogInformation("Setting changed: Comment = \"{Comment}\" ({UserName})", update.Comment, userName);
				}
				foreach (PoolId addedPool in newAgent.ExplicitPools.Except(agent.ExplicitPools))
				{
					logger.LogInformation("Added to pool {PoolId} ({UserName})", addedPool, userName);
				}
				foreach (PoolId removedPool in agent.ExplicitPools.Except(newAgent.ExplicitPools))
				{
					logger.LogInformation("Removed from pool {PoolId} ({UserName})", removedPool, userName);
				}
				break;
			}
			return Ok();
		}

		/// <summary>
		/// Remove a registered agent.
		/// </summary>
		/// <param name="agentId">Id of the agent to delete.</param>
		/// <returns>Http result code</returns>
		[HttpDelete]
		[Route("/api/v1/agents/{agentId}")]
		public async Task<ActionResult> DeleteAgentAsync(AgentId agentId)
		{
			if (!_globalConfig.Value.Authorize(AgentAclAction.DeleteAgent, User))
			{
				return Forbid(AgentAclAction.DeleteAgent, agentId);
			}

			IAgent? agent = await _agentService.GetAgentAsync(agentId);
			if (agent == null)
			{
				return NotFound(agentId);
			}

			await _agentService.DeleteAgentAsync(agent);
			return new OkResult();
		}

		/// <summary>
		/// Retrieve historical information about a specific agent
		/// </summary>
		/// <param name="agentId">Id of the agent to get information about</param>
		/// <param name="minTime">Minimum time for records to return</param>
		/// <param name="maxTime">Maximum time for records to return</param>
		/// <param name="index">Offset of the first result</param>
		/// <param name="count">Number of records to return</param>
		/// <returns>Information about the requested agent</returns>
		[HttpGet]
		[Route("/api/v1/agents/{agentId}/history")]
		public async Task GetAgentHistoryAsync(AgentId agentId, [FromQuery] DateTime? minTime = null, [FromQuery] DateTime? maxTime = null, [FromQuery] int index = 0, [FromQuery] int count = 50)
		{
			Response.ContentType = "application/json";
			Response.StatusCode = 200;
			await Response.StartAsync();
			await _agentService.Agents.GetLogger(agentId).FindAsync(Response.BodyWriter, minTime, maxTime, index, count);
		}

		/// <summary>
		/// Find all the sessions of a particular agent
		/// </summary>
		/// <param name="agentId">Unique id of the agent to find</param>
		/// <param name="startTime">Start time to include in the search</param>
		/// <param name="finishTime">Finish time to include in the search</param>
		/// <param name="index">Index of the first result to return</param>
		/// <param name="count">Number of results to return</param>
		/// <returns>Sessions </returns>
		[HttpGet]
		[Route("/api/v1/agents/{agentId}/sessions")]
		public async Task<ActionResult<List<GetAgentSessionResponse>>> FindSessionsAsync(AgentId agentId, [FromQuery] DateTimeOffset? startTime, [FromQuery] DateTimeOffset? finishTime, [FromQuery] int index = 0, [FromQuery] int count = 50)
		{
			if (!_globalConfig.Value.Authorize(SessionAclAction.ViewSession, User))
			{
				return Forbid(SessionAclAction.ViewSession, agentId);
			}

			IAgent? agent = await _agentService.GetAgentAsync(agentId);
			if (agent == null)
			{
				return NotFound(agentId);
			}

			List<ISession> sessions = await _agentService.FindSessionsAsync(agentId, startTime?.UtcDateTime, finishTime?.UtcDateTime, index, count);
			return sessions.ConvertAll(x => CreateGetAgentSessionResponse(x));
		}

		static GetAgentSessionResponse CreateGetAgentSessionResponse(ISession session)
		{
			return new GetAgentSessionResponse(session.Id, session.StartTime, session.FinishTime, (session.Properties != null) ? new List<string>(session.Properties) : null, session.Version);
		}

		/// <summary>
		/// Find all the sessions of a particular agent
		/// </summary>
		/// <param name="agentId">Unique id of the agent to find</param>
		/// <param name="sessionId">Unique id of the session</param>
		/// <returns>Sessions </returns>
		[HttpGet]
		[Route("/api/v1/agents/{agentId}/sessions/{sessionId}")]
		public async Task<ActionResult<GetAgentSessionResponse>> GetSessionAsync(AgentId agentId, SessionId sessionId)
		{
			if (!_globalConfig.Value.Authorize(SessionAclAction.ViewSession, User))
			{
				return Forbid(SessionAclAction.ViewSession, agentId);
			}

			IAgent? agent = await _agentService.GetAgentAsync(agentId);
			if (agent == null)
			{
				return NotFound(agentId);
			}

			ISession? session = await _agentService.GetSessionAsync(sessionId);
			if (session == null || session.AgentId != agentId)
			{
				return NotFound();
			}

			return CreateGetAgentSessionResponse(session);
		}

		/// <summary>
		/// Find all the leases for a particular agent
		/// </summary>
		/// <param name="agentId">Unique id of the agent to find</param>
		/// <param name="sessionId">The session to query</param>
		/// <param name="startTime">Start of the time window to consider</param>
		/// <param name="finishTime">End of the time window to consider</param>
		/// <param name="index">Index of the first result to return</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="filter">Filter to apply to the properties</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Sessions </returns>
		[HttpGet]
		[Route("/api/v1/agents/{agentId}/leases")]
		[ProducesResponseType(200, Type = typeof(List<GetAgentLeaseResponse>))]
		public async Task<ActionResult<List<object>>> FindLeasesAsync(AgentId agentId, [FromQuery] SessionId? sessionId, [FromQuery] DateTimeOffset? startTime, [FromQuery] DateTimeOffset? finishTime, [FromQuery] int index = 0, [FromQuery] int count = 1000, [FromQuery] PropertyFilter? filter = null, CancellationToken cancellationToken = default)
		{
			if (!_globalConfig.Value.Authorize(LeaseAclAction.ViewLeases, User))
			{
				return Forbid(LeaseAclAction.ViewLeases, agentId);
			}

			IAgent? agent = await _agentService.GetAgentAsync(agentId, cancellationToken);
			if (agent == null)
			{
				return NotFound(agentId);
			}

			IReadOnlyList<ILease> leases = await _agentService.FindLeasesAsync(agentId, sessionId, startTime?.UtcDateTime, finishTime?.UtcDateTime, index, count, cancellationToken);

			double? agentRate = null;
			if (_globalConfig.Value.Authorize(ServerAclAction.ViewCosts, User))
			{
				agentRate = await _agentService.GetRateAsync(agentId, cancellationToken);
			}

			List<object> responses = new List<object>();
			foreach (ILease lease in leases)
			{
				Dictionary<string, string>? details = await _agentService.GetPayloadDetailsAsync(lease.Payload, cancellationToken);
				responses.Add(PropertyFilter.Apply(CreateGetAgentLeaseResponse(lease, details, agentRate), filter));
			}

			return responses;
		}

		/// <summary>
		/// Get info about a particular lease
		/// </summary>
		/// <param name="agentId">Unique id of the agent to find</param>
		/// <param name="leaseId">Unique id of the particular lease</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Lease matching the given id</returns>
		[HttpGet]
		[Route("/api/v1/agents/{agentId}/leases/{leaseId}")]
		public async Task<ActionResult<GetAgentLeaseResponse>> GetLeaseAsync(AgentId agentId, LeaseId leaseId, CancellationToken cancellationToken = default)
		{
			if (!_globalConfig.Value.Authorize(LeaseAclAction.ViewLeases, User))
			{
				return Forbid(LeaseAclAction.ViewLeases, agentId);
			}

			IAgent? agent = await _agentService.GetAgentAsync(agentId, cancellationToken);
			if (agent == null)
			{
				return NotFound(agentId);
			}

			ILease? lease = await _agentService.GetLeaseAsync(leaseId, cancellationToken);
			if (lease == null || lease.AgentId != agentId)
			{
				return NotFound(agentId, leaseId);
			}

			double? agentRate = null;
			if (_globalConfig.Value.Authorize(ServerAclAction.ViewCosts, User))
			{
				agentRate = await _agentService.GetRateAsync(agentId, cancellationToken);
			}

			Dictionary<string, string>? details = await _agentService.GetPayloadDetailsAsync(lease.Payload, cancellationToken);
			return CreateGetAgentLeaseResponse(lease, details, agentRate);
		}
	}
}
