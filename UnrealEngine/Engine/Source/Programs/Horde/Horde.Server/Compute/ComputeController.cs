// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Net;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Compute;
using Horde.Server.Acls;
using Horde.Server.Server;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;

namespace Horde.Server.Compute
{
	/// <summary>
	/// Request a machine to execute compute requests
	/// </summary>
	public class AssignComputeRequest
	{
		/// <summary>
		/// Condition to identify machines that can execute the request
		/// </summary>
		public Requirements? Requirements { get; set; }
	}

	/// <summary>
	/// Request a machine to execute compute requests
	/// </summary>
	public class AssignComputeResponse
	{
		/// <summary>
		/// IP address of the remote machine
		/// </summary>
		public string Ip { get; set; } = String.Empty;

		/// <summary>
		/// Port number on the remote machine
		/// </summary>
		public int Port { get; set; }

		/// <summary>
		/// Cryptographic nonce to identify the request, as a hex string
		/// </summary>
		public string Nonce { get; set; } = String.Empty;

		/// <summary>
		/// AES key for the channel, as a hex string
		/// </summary>
		public string Key { get; set; } = String.Empty;

		/// <summary>
		/// Resources assigned to this machine
		/// </summary>
		public Dictionary<string, int> AssignedResources { get; set; } = new Dictionary<string, int>();

		/// <summary>
		/// Properties of the agent assigned to do the work
		/// </summary>
		public IReadOnlyList<string> Properties { get; set; } = new List<string>();
	}

	/// <summary>
	/// Controller for the /api/v2/compute endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class ComputeControllerV2 : HordeControllerBase
	{
		readonly ComputeService _computeService;
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeControllerV2(ComputeService computeService, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_computeService = computeService;
			_globalConfig = globalConfig;
		}

		/// <summary>
		/// Add tasks to be executed remotely
		/// </summary>
		/// <param name="clusterId">Id of the compute cluster</param>
		/// <param name="request">The request parameters</param>
		/// <returns></returns>
		[HttpPost]
		[Authorize]
		[Route("/api/v2/compute/{clusterId}")]
		public async Task<ActionResult<AssignComputeResponse>> AssignComputeResourceAsync(ClusterId clusterId, [FromBody] AssignComputeRequest request)
		{
			ComputeClusterConfig? clusterConfig;
			if (!_globalConfig.Value.TryGetComputeCluster(clusterId, out clusterConfig))
			{
				return NotFound(clusterId);
			}
			if(!clusterConfig.Authorize(ComputeAclAction.AddComputeTasks, User))
			{
				return Forbid(ComputeAclAction.AddComputeTasks, clusterId);
			}

			Requirements requirements = request.Requirements ?? new Requirements();

			ComputeResource? computeResource = await _computeService.TryAllocateResource(requirements);
			if (computeResource == null)
			{
				return StatusCode((int)HttpStatusCode.ServiceUnavailable);
			}

			AssignComputeResponse response = new AssignComputeResponse();
			response.Ip = computeResource.Ip.ToString();
			response.Port = computeResource.Port;
			response.Nonce = StringUtils.FormatHexString(computeResource.Task.Nonce.Span);
			response.Key = StringUtils.FormatHexString(computeResource.Task.Key.Span);
			response.Properties = computeResource.Properties;

			foreach (KeyValuePair<string, int> pair in computeResource.Task.Resources)
			{
				response.AssignedResources.Add(pair.Key, pair.Value);
			}

			return response;
		}
	}
}
