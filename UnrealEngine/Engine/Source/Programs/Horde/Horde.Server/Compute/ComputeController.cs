// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Agents.Pools;
using EpicGames.Horde.Compute;
using Horde.Server.Acls;
using Horde.Server.Agents;
using Horde.Server.Server;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;

namespace Horde.Server.Compute
{
	/// <summary>
	/// Controller for the /api/v2/compute endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class ComputeController : HordeControllerBase
	{
		readonly ComputeService _computeService;
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeController(ComputeService computeService, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_computeService = computeService;
			_globalConfig = globalConfig;
		}

		/// <summary>
		/// Add tasks to be executed remotely
		/// </summary>
		/// <param name="clusterId">Id of the compute cluster</param>
		/// <param name="request">The request parameters</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		[HttpPost]
		[Authorize]
		[Route("/api/v2/compute/{clusterId}")]
		public async Task<ActionResult<AssignComputeResponse>> AssignComputeResourceAsync(ClusterId clusterId, [FromBody] AssignComputeRequest request, CancellationToken cancellationToken)
		{
			if (!_globalConfig.Value.TryGetComputeCluster(clusterId, out ComputeClusterConfig? clusterConfig))
			{
				return NotFound(clusterId);
			}
			if (!clusterConfig.Authorize(ComputeAclAction.AddComputeTasks, User))
			{
				return Forbid(ComputeAclAction.AddComputeTasks, clusterId);
			}

			AllocateResourceParams arp = new(clusterId, (ComputeProtocol)request.Protocol, request.Requirements)
			{
				RequestId = request.RequestId,
				RequesterIp = HttpContext.Connection.RemoteIpAddress,
				ParentLeaseId = User.GetLeaseClaim(),
				Ports = request.Connection?.Ports ?? new Dictionary<string, int>(),
				ConnectionMode = request.Connection?.ModePreference,
				RequesterPublicIp = request.Connection?.ClientPublicIp,
				UsePublicIp = request.Connection?.PreferPublicIp,
				Encryption = ComputeService.ConvertEncryptionToProto(request.Connection?.Encryption)
			};

			ComputeResource? computeResource;
			try
			{
				computeResource = await _computeService.TryAllocateResourceAsync(arp, cancellationToken);
				if (computeResource == null)
				{
					return StatusCode((int)HttpStatusCode.ServiceUnavailable, "No resources available");
				}
			}
			catch (ComputeServiceException cse)
			{
				return cse.ShowToUser ? StatusCode((int)HttpStatusCode.InternalServerError, cse.Message) : StatusCode((int)HttpStatusCode.InternalServerError);
			}

			Dictionary<string, ConnectionMetadataPort> responsePorts = new();
			foreach ((string name, ComputeResourcePort crp) in computeResource.Ports)
			{
				responsePorts[name] = new ConnectionMetadataPort(crp.Port, crp.AgentPort);
			}

			AssignComputeResponse response = new AssignComputeResponse();
			response.Ip = computeResource.Ip.ToString();
			response.Port = computeResource.Ports[ConnectionMetadataPort.ComputeId].Port;
			response.ConnectionMode = computeResource.ConnectionMode;
			response.ConnectionAddress = computeResource.ConnectionAddress;
			response.Ports = responsePorts;
			response.Encryption = ComputeService.ConvertEncryptionFromProto(computeResource.Task.Encryption);
			response.Nonce = StringUtils.FormatHexString(computeResource.Task.Nonce.Span);
			response.Key = StringUtils.FormatHexString(computeResource.Task.Key.Span);
			response.Certificate = StringUtils.FormatHexString(computeResource.Task.Certificate.Span);
			response.AgentId = computeResource.AgentId;
			response.LeaseId = computeResource.LeaseId;
			response.Properties = computeResource.Properties;
			response.Protocol = computeResource.Task.Protocol;

			foreach (KeyValuePair<string, int> pair in computeResource.Task.Resources)
			{
				response.AssignedResources.Add(pair.Key, pair.Value);
			}

			return response;
		}

		/// <summary>
		/// Get current resource needs for active sessions
		/// </summary>
		/// <param name="clusterId">ID of the compute cluster</param>
		/// <returns>List of resource needs</returns>
		[HttpGet]
		[Authorize]
		[Route("/api/v2/compute/{clusterId}/resource-needs")]
		public async Task<ActionResult<GetResourceNeedsResponse>> GetResourceNeedsAsync(ClusterId clusterId)
		{
			if (!_globalConfig.Value.TryGetComputeCluster(clusterId, out ComputeClusterConfig? clusterConfig))
			{
				return NotFound(clusterId);
			}

			if (!clusterConfig.Authorize(ComputeAclAction.GetComputeTasks, User))
			{
				return Forbid(ComputeAclAction.GetComputeTasks, clusterId);
			}

			List<ResourceNeedsMessage> resourceNeeds =
				(await _computeService.GetResourceNeedsAsync())
				.Where(x => x.ClusterId == clusterId.ToString())
				.OrderBy(x => x.Timestamp)
				.Select(x => new ResourceNeedsMessage { SessionId = x.SessionId, Pool = x.Pool, ResourceNeeds = x.ResourceNeeds })
				.ToList();

			return new GetResourceNeedsResponse { ResourceNeeds = resourceNeeds };
		}

		/// <summary>
		/// Declare resource needs for a session to help server calculate current demand
		/// <see cref="KnownPropertyNames"/> for resource name property names
		/// </summary>
		/// <param name="clusterId">Id of the compute cluster</param>
		/// <param name="request">Resource needs request</param>
		/// <returns></returns>
		[HttpPost]
		[Authorize]
		[Route("/api/v2/compute/{clusterId}/resource-needs")]
		public async Task<ActionResult<AssignComputeResponse>> SetResourceNeedsAsync(ClusterId clusterId, [FromBody] ResourceNeedsMessage request)
		{
			if (!_globalConfig.Value.TryGetComputeCluster(clusterId, out ComputeClusterConfig? clusterConfig))
			{
				return NotFound(clusterId);
			}

			if (!clusterConfig.Authorize(ComputeAclAction.AddComputeTasks, User))
			{
				return Forbid(ComputeAclAction.AddComputeTasks, clusterId);
			}

			await _computeService.SetResourceNeedsAsync(clusterId, request.SessionId, new PoolId(request.Pool).ToString(), request.ResourceNeeds);
			return Ok(new { message = "Resource needs set" });
		}
	}
}
