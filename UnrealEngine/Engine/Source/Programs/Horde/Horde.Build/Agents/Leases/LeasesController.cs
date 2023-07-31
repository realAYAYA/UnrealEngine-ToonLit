// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Horde.Build.Acls;
using Horde.Build.Agents.Sessions;
using Horde.Build.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using OpenTracing;
using OpenTracing.Util;

namespace Horde.Build.Agents.Leases
{
	using LeaseId = ObjectId<ILease>;
	using SessionId = ObjectId<ISession>;

	/// <summary>
	/// Controller for the /api/v1/leases endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class LeasesController : HordeControllerBase
	{
		/// <summary>
		/// Singleton instance of the ACL service
		/// </summary>
		readonly AclService _aclService;

		/// <summary>
		/// Singleton instance of the agent service
		/// </summary>
		readonly AgentService _agentService;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="aclService">The ACL service singleton</param>
		/// <param name="agentService">The agent service</param>
		public LeasesController(AclService aclService, AgentService agentService)
		{
			_aclService = aclService;
			_agentService = agentService;
		}

		/// <summary>
		/// Find all the leases for a particular agent
		/// </summary>
		/// <param name="agentId">Unique id of the agent to find</param>
		/// <param name="sessionId">The session to query</param>
		/// <param name="startTime">Start of the time window to consider</param>
		/// <param name="finishTime">End of the time window to consider</param>
		/// <param name="minFinishTime">Start of the time window to consider when querying by finish time (if set, other criteria are ignored)</param>
		/// <param name="maxFinishTime">End of the time window to consider when querying by finish time (if set, other criteria are ignored)</param>
		/// <param name="index">Index of the first result to return</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="filter">Filter to apply to the results</param>
		/// <returns>Sessions </returns>
		[HttpGet]
		[Route("/api/v1/leases")]
		[ProducesResponseType(200, Type = typeof(List<GetAgentLeaseResponse>))]
		public async Task<ActionResult<List<object>>> FindLeasesAsync(
			[FromQuery] AgentId? agentId,
			[FromQuery] SessionId? sessionId,
			[FromQuery] DateTimeOffset? startTime,
			[FromQuery] DateTimeOffset? finishTime,
			[FromQuery] DateTimeOffset? minFinishTime,
			[FromQuery] DateTimeOffset? maxFinishTime,
			[FromQuery] int index = 0,
			[FromQuery] int count = 1000,
			[FromQuery] PropertyFilter? filter = null)
		{
			GlobalPermissionsCache permissionsCache = new GlobalPermissionsCache();
			if (agentId == null)
			{
				if (!await _aclService.AuthorizeAsync(AclAction.ViewLeases, User, permissionsCache))
				{
					return Forbid(AclAction.ViewLeases);
				}
			}
			else
			{
				IAgent? agent = await _agentService.GetAgentAsync(agentId.Value);
				if (agent == null)
				{
					return NotFound(agentId.Value);
				}
				if (!await _agentService.AuthorizeAsync(agent, AclAction.ViewLeases, User, permissionsCache))
				{
					return Forbid(AclAction.ViewLeases, agentId.Value);
				}
			}

			bool includeCosts = await _aclService.AuthorizeAsync(AclAction.ViewCosts, User, permissionsCache);

			List<ILease> leases;
			if (minFinishTime == null && maxFinishTime == null)
			{
				leases = await _agentService.FindLeasesAsync(agentId, sessionId, startTime?.UtcDateTime, finishTime?.UtcDateTime, index, count);
			}
			else
			{
				// Optimized path for queries made by finish time
				leases = await _agentService.FindLeasesByFinishTimeAsync(minFinishTime?.UtcDateTime, maxFinishTime?.UtcDateTime, index, count);
			}

			List<object> responses = new List<object>();

			using (IScope _ = GlobalTracer.Instance.BuildSpan($"GenerateResponses").StartActive())
			{
				Dictionary<AgentId, double?> cachedAgentRates = new Dictionary<AgentId, double?>();
				foreach (ILease lease in leases)
				{
					double? agentRate = null;
					if (includeCosts && !cachedAgentRates.TryGetValue(lease.AgentId, out agentRate))
					{
						agentRate = await _agentService.GetRateAsync(lease.AgentId);
						cachedAgentRates.Add(lease.AgentId, agentRate);
					}

					Dictionary<string, string>? details = _agentService.GetPayloadDetails(lease.Payload);
					responses.Add(PropertyFilter.Apply(new GetAgentLeaseResponse(lease, details, agentRate), filter));
				}
			}

			return responses;
		}

		/// <summary>
		/// Get info about a particular lease
		/// </summary>
		/// <param name="leaseId">Unique id of the particular lease</param>
		/// <returns>Lease matching the given id</returns>
		[HttpGet]
		[Route("/api/v1/leases/{leaseId}")]
		public async Task<ActionResult<GetAgentLeaseResponse>> GetLeaseAsync(LeaseId leaseId)
		{
			ILease? lease = await _agentService.GetLeaseAsync(leaseId);
			if (lease == null)
			{
				return NotFound(leaseId);
			}

			IAgent? agent = await _agentService.GetAgentAsync(lease.AgentId);
			if (agent == null)
			{
				return NotFound(lease.AgentId);
			}

			GlobalPermissionsCache permissionsCache = new GlobalPermissionsCache();
			if (!await _agentService.AuthorizeAsync(agent, AclAction.ViewLeases, User, permissionsCache))
			{
				return Forbid(AclAction.ViewLeases);
			}

			double? agentRate = null;
			if (await _aclService.AuthorizeAsync(AclAction.ViewCosts, User, permissionsCache))
			{
				agentRate = await _agentService.GetRateAsync(agent.Id);
			}

			Dictionary<string, string>? details = _agentService.GetPayloadDetails(lease.Payload);
			return new GetAgentLeaseResponse(lease, details, agentRate);
		}

		/// <summary>
		/// Update a particular lease
		/// </summary>
		/// <param name="leaseId">Unique id of the particular lease</param>
		/// <param name="request"></param>
		/// <returns>Lease matching the given id</returns>
		[HttpPut]
		[Route("/api/v1/leases/{leaseId}")]
		public async Task<ActionResult> UpdateLeaseAsync(LeaseId leaseId, [FromBody] UpdateLeaseRequest request)
		{
			// only update supported right now is abort
			if (!request.Aborted.HasValue || !request.Aborted.Value)
			{
				return Ok();
			}

			ILease? lease = await _agentService.GetLeaseAsync(leaseId);
			if (lease == null)
			{
				return NotFound(leaseId);
			}

			IAgent? agent = await _agentService.GetAgentAsync(lease.AgentId);
			if (agent == null)
			{
				return NotFound(lease.AgentId);
			}

			if (!await _agentService.AuthorizeAsync(agent, AclAction.AdminWrite, User, null))
			{
				return Forbid(AclAction.AdminWrite, lease.AgentId);
			}

			AgentLease? agentLease = agent.Leases.FirstOrDefault(x => x.Id == leaseId);

			if (agentLease == null)
			{
				return NotFound(agent.Id, leaseId);
			}

			if (!agentLease.IsConformLease())
			{
				return BadRequest("Lease abort only supported on conform leases for now, {LeaseId}", leaseId);
			}

			await _agentService.CancelLeaseAsync(agent, leaseId);
			return Ok();
		}
	}
}
