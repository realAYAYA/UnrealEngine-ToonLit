// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Horde.Server.Acls;
using Horde.Server.Agents.Sessions;
using Horde.Server.Server;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;
using OpenTelemetry.Trace;

namespace Horde.Server.Agents.Leases
{
	/// <summary>
	/// Controller for the /api/v1/leases endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class LeasesController : HordeControllerBase
	{
		readonly AgentService _agentService;
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;
		readonly Tracer _tracer;

		/// <summary>
		/// Constructor
		/// </summary>
		public LeasesController(AgentService agentService, IOptionsSnapshot<GlobalConfig> globalConfig, Tracer tracer)
		{
			_agentService = agentService;
			_globalConfig = globalConfig;
			_tracer = tracer;
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
			if (!_globalConfig.Value.Authorize(LeaseAclAction.ViewLeases, User))
			{
				return Forbid(LeaseAclAction.ViewLeases);
			}

			bool includeCosts = _globalConfig.Value.Authorize(ServerAclAction.ViewCosts, User);

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

			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(LeasesController)}.{nameof(FindLeasesAsync)}");
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
			if (!_globalConfig.Value.Authorize(LeaseAclAction.ViewLeases, User))
			{
				return Forbid(LeaseAclAction.ViewLeases);
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

			double? agentRate = null;
			if (_globalConfig.Value.Authorize(ServerAclAction.ViewCosts, User))
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
			if (!_globalConfig.Value.Authorize(AdminAclAction.AdminWrite, User))
			{
				return Forbid(AdminAclAction.AdminWrite);
			}

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
