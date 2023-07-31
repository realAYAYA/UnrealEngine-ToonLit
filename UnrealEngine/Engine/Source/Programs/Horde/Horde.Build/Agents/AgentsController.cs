// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Horde.Build.Acls;
using Horde.Build.Agents.Leases;
using Horde.Build.Agents.Pools;
using Horde.Build.Agents.Sessions;
using Horde.Build.Agents.Software;
using Horde.Build.Auditing;
using Horde.Build.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Logging;

namespace Horde.Build.Agents
{
	using AgentSoftwareChannelName = StringId<AgentSoftwareChannels>;
	using LeaseId = ObjectId<ILease>;
	using PoolId = StringId<IPool>;
	using SessionId = ObjectId<ISession>;

	/// <summary>
	/// Controller for the /api/v1/agents endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class AgentsController : HordeControllerBase
	{
		readonly AclService _aclService;
		readonly AgentService _agentService;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="aclService">The ACL service singleton</param>
		/// <param name="agentService">The agent service</param>
		public AgentsController(AclService aclService, AgentService agentService)
		{
			_aclService = aclService;
			_agentService = agentService;
		}

		/// <summary>
		/// Register an agent to perform remote work.
		/// </summary>
		/// <param name="request">Request parameters</param>
		/// <returns>Information about the registered agent</returns>
		[HttpPost]
		[Route("/api/v1/agents")]
		public async Task<ActionResult<CreateAgentResponse>> CreateAgentAsync([FromBody] CreateAgentRequest request)
		{
			if(!await _aclService.AuthorizeAsync(AclAction.CreateAgent, User))
			{
				return Forbid(AclAction.CreateAgent);
			}

			AgentSoftwareChannelName? channel = String.IsNullOrEmpty(request.Channel) ? (AgentSoftwareChannelName?)null : new AgentSoftwareChannelName(request.Channel);
			IAgent agent = await _agentService.CreateAgentAsync(request.Name, request.Enabled, channel, request.Pools?.ConvertAll(x => new PoolId(x)));

			return new CreateAgentResponse(agent.Id.ToString());
		}

		/// <summary>
		/// Finds the agents matching specified criteria.
		/// </summary>
		/// <param name="poolId">The pool containing the agent</param>
		/// <param name="index">First result to return</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="modifiedAfter">If set, only returns agents modified after this time</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>List of matching agents</returns>
		[HttpGet]
		[Route("/api/v1/agents")]
		[ProducesResponseType(typeof(List<GetAgentResponse>), 200)]
		public async Task<ActionResult<List<object>>> FindAgentsAsync([FromQuery] PoolId? poolId = null, [FromQuery] int? index = null, [FromQuery] int? count = null, [FromQuery] DateTimeOffset? modifiedAfter = null, [FromQuery] PropertyFilter? filter = null)
		{
			GlobalPermissionsCache permissionsCache = new GlobalPermissionsCache();
			List<IAgent> agents = await _agentService.FindAgentsAsync(poolId, modifiedAfter?.UtcDateTime, index, count);

			List<object> responses = new List<object>();
			foreach (IAgent agent in agents)
			{
				responses.Add(await GetAgentResponseAsync(agent, permissionsCache, filter));
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
			IAgent? agent = await _agentService.GetAgentAsync(agentId);
			if (agent == null)
			{
				return NotFound(agentId);
			}

			return await GetAgentResponseAsync(agent, new GlobalPermissionsCache(), filter);
		}

		/// <summary>
		/// Gets an individual agent response
		/// </summary>
		async ValueTask<object> GetAgentResponseAsync(IAgent agent, GlobalPermissionsCache permissionsCache, PropertyFilter? filter = null)
		{
			double? rate = null;
			if (await _agentService.AuthorizeAsync(agent, AclAction.ViewCosts, User, permissionsCache))
			{
				rate = await _agentService.GetRateAsync(agent.Id);
			}

			List<GetAgentLeaseResponse> leases = new List<GetAgentLeaseResponse>();
			foreach (AgentLease lease in agent.Leases)
			{
				Dictionary<string, string>? details = _agentService.GetPayloadDetails(lease.Payload);
				leases.Add(new GetAgentLeaseResponse(lease, details));
			}

			bool bIncludeAcl = await _agentService.AuthorizeAsync(agent, AclAction.ViewPermissions, User, permissionsCache);
			return new GetAgentResponse(agent, leases, rate, bIncludeAcl).ApplyFilter(filter);
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
			List<PoolId>? updatePools = update.Pools?.ConvertAll(x => new PoolId(x));
			AgentSoftwareChannelName? channel = String.IsNullOrEmpty(update.Channel) ? (AgentSoftwareChannelName?)null : new AgentSoftwareChannelName(update.Channel);

			string userName = User.GetUserName() ?? "Unknown";

			GlobalPermissionsCache cache = new GlobalPermissionsCache();
			for (; ; )
			{
				IAgent? agent = await _agentService.GetAgentAsync(agentId);
				if (agent == null)
				{
					return NotFound(agentId);
				}
				if (!await _agentService.AuthorizeAsync(agent, AclAction.UpdateAgent, User, cache))
				{
					return Forbid(AclAction.UpdateAgent, agentId);
				}
				if (update.Acl != null && !await _agentService.AuthorizeAsync(agent, AclAction.ChangePermissions, User, cache))
				{
					return Forbid(AclAction.ChangePermissions, agentId);
				}

				IAgent? newAgent = await _agentService.Agents.TryUpdateSettingsAsync(agent, update.Enabled, update.RequestConform, update.RequestFullConform, update.RequestRestart, update.RequestShutdown, $"Manual ({userName})", channel, update.Pools?.ConvertAll(x => new PoolId(x)), Acl.Merge(agent.Acl, update.Acl), update.Comment);
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
			IAgent? agent = await _agentService.GetAgentAsync(agentId);
			if (agent == null)
			{
				return NotFound(agentId);
			}
			if (!await _agentService.AuthorizeAsync(agent, AclAction.DeleteAgent, User, null))
			{
				return Forbid(AclAction.DeleteAgent, agentId);
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
			IAgent? agent = await _agentService.GetAgentAsync(agentId);
			if (agent == null)
			{
				return NotFound(agentId);
			}
			if (!await _agentService.AuthorizeAsync(agent, AclAction.ViewSession, User, null))
			{
				return Forbid(AclAction.ViewSession, agentId);
			}

			List<ISession> sessions = await _agentService.FindSessionsAsync(agentId, startTime?.UtcDateTime, finishTime?.UtcDateTime, index, count);
			return sessions.ConvertAll(x => new GetAgentSessionResponse(x));
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
			IAgent? agent = await _agentService.GetAgentAsync(agentId);
			if (agent == null)
			{
				return NotFound(agentId);
			}
			if (!await _agentService.AuthorizeAsync(agent, AclAction.ViewSession, User, null))
			{
				return Forbid(AclAction.ViewSession, agentId);
			}

			ISession? session = await _agentService.GetSessionAsync(sessionId);
			if(session == null || session.AgentId != agentId)
			{
				return NotFound();
			}

			return new GetAgentSessionResponse(session);
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
		/// <returns>Sessions </returns>
		[HttpGet]
		[Route("/api/v1/agents/{agentId}/leases")]
		[ProducesResponseType(200, Type = typeof(List<GetAgentLeaseResponse>))]
		public async Task<ActionResult<List<object>>> FindLeasesAsync(AgentId agentId, [FromQuery] SessionId? sessionId, [FromQuery] DateTimeOffset? startTime, [FromQuery] DateTimeOffset? finishTime, [FromQuery] int index = 0, [FromQuery] int count = 1000, [FromQuery] PropertyFilter? filter = null)
		{
			IAgent? agent = await _agentService.GetAgentAsync(agentId);
			if (agent == null)
			{
				return NotFound(agentId);
			}
			if (!await _agentService.AuthorizeAsync(agent, AclAction.ViewLeases, User, null))
			{
				return Forbid(AclAction.ViewLeases, agentId);
			}

			List<ILease> leases = await _agentService.FindLeasesAsync(agentId, sessionId, startTime?.UtcDateTime, finishTime?.UtcDateTime, index, count);

			double? agentRate = null;
			if (await _aclService.AuthorizeAsync(AclAction.ViewCosts, User))
			{
				agentRate = await _agentService.GetRateAsync(agentId);
			}

			List<object> responses = new List<object>();
			foreach(ILease lease in leases)
			{
				Dictionary<string, string>? details = _agentService.GetPayloadDetails(lease.Payload);
				responses.Add(PropertyFilter.Apply(new GetAgentLeaseResponse(lease, details, agentRate), filter));
			}

			return responses;
		}

		/// <summary>
		/// Get info about a particular lease
		/// </summary>
		/// <param name="agentId">Unique id of the agent to find</param>
		/// <param name="leaseId">Unique id of the particular lease</param>
		/// <returns>Lease matching the given id</returns>
		[HttpGet]
		[Route("/api/v1/agents/{agentId}/leases/{leaseId}")]
		public async Task<ActionResult<GetAgentLeaseResponse>> GetLeaseAsync(AgentId agentId, LeaseId leaseId)
		{
			IAgent? agent = await _agentService.GetAgentAsync(agentId);
			if(agent == null)
			{
				return NotFound(agentId);
			}
			if (!await _aclService.AuthorizeAsync(AclAction.ViewLeases, User))
			{
				return Forbid(AclAction.ViewLeases, agentId);
			}

			ILease? lease = await _agentService.GetLeaseAsync(leaseId);
			if (lease == null || lease.AgentId != agentId)
			{
				return NotFound(agentId, leaseId);
			}

			double? agentRate = null;
			if (await _aclService.AuthorizeAsync(AclAction.ViewCosts, User))
			{
				agentRate = await _agentService.GetRateAsync(agentId);
			}

			Dictionary<string, string>? details = _agentService.GetPayloadDetails(lease.Payload);
			return new GetAgentLeaseResponse(lease, details, agentRate);
		}
	}
}
