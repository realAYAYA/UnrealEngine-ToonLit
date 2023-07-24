// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Net;
using EpicGames.Core;
using EpicGames.Horde.Compute;
using Horde.Build.Acls;
using Horde.Build.Server;
using Horde.Build.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;

namespace Horde.Build.Compute.V2
{
	/// <summary>
	/// Request a machine to execute compute requests
	/// </summary>
	public class AddComputeTaskRequest
	{
		/// <summary>
		/// Condition to identify machines that can execute the request
		/// </summary>
		public Requirements? Requirements { get; set; }

		/// <summary>
		/// Port to connect on
		/// </summary>
		public int? RemotePort { get; set; }

		/// <summary>
		/// Cryptographic nonce to identify the request, as a hex string
		/// </summary>
		public string Nonce { get; set; } = String.Empty;

		/// <summary>
		/// AES key for the channel, as a hex string
		/// </summary>
		public string AesKey { get; set; } = String.Empty;

		/// <summary>
		/// AES IV for the channel, as a hex string
		/// </summary>
		public string AesIv { get; set; } = String.Empty;
	}

	/// <summary>
	/// Request a machine to execute compute requests
	/// </summary>
	public class AddComputeTasksRequest
	{
		/// <summary>
		/// Condition to identify machines that can execute the request
		/// </summary>
		public Requirements? Requirements { get; set; }

		/// <summary>
		/// Port to connect on
		/// </summary>
		public int RemotePort { get; set; }

		/// <summary>
		/// List of tasks to add
		/// </summary>
		public List<AddComputeTaskRequest> Tasks { get; set; } = new List<AddComputeTaskRequest>();
	}

	/// <summary>
	/// Controller for the /api/v2/compute endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class ComputeControllerV2 : HordeControllerBase
	{
		readonly ComputeServiceV2 _computeService;
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeControllerV2(ComputeServiceV2 computeService, IOptionsSnapshot<GlobalConfig> globalConfig)
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
		public ActionResult AddTasksAsync(ClusterId clusterId, [FromBody] AddComputeTasksRequest request)
		{
			ComputeClusterConfig? clusterConfig;
			if (!_globalConfig.Value.TryGetComputeCluster(clusterId, out clusterConfig))
			{
				return NotFound(clusterId);
			}
			if(!clusterConfig.Authorize(AclAction.AddComputeTasks, User))
			{
				return Forbid(AclAction.AddComputeTasks, clusterId);
			}

			IPAddress? remoteIp = HttpContext.Connection.RemoteIpAddress;
			if (remoteIp == null)
			{
				return BadRequest("Missing remote IP address");
			}

			foreach (AddComputeTaskRequest taskRequest in request.Tasks)
			{
				Requirements requirements = taskRequest.Requirements ?? request.Requirements ?? new Requirements();

				int port = taskRequest.RemotePort ?? request.RemotePort;
				byte[] nonce = StringUtils.ParseHexString(taskRequest.Nonce);
				byte[] aesKey = StringUtils.ParseHexString(taskRequest.AesKey);
				byte[] aesIv = StringUtils.ParseHexString(taskRequest.AesIv);

				_computeService.AddRequest(clusterId, requirements, remoteIp.ToString(), port, nonce, aesKey, aesIv);
			}
			return Ok();
		}
	}
}
