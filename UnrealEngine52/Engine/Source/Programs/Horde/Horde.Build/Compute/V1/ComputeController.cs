// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Compute.Impl;
using Horde.Build.Acls;
using Horde.Build.Server;
using Horde.Build.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;

namespace Horde.Build.Compute.V1
{
	/// <summary>
	/// Controller for the /api/v1/compute endpoint
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
		/// Gets information about a cluster
		/// </summary>
		/// <param name="clusterId">The cluster to add to</param>
		/// <returns></returns>
		[HttpGet]
		[Authorize]
		[Route("/api/v1/compute/{clusterId}")]
		public async Task<ActionResult<GetComputeClusterInfo>> GetClusterInfoAsync([FromRoute] ClusterId clusterId)
		{
			if (!_globalConfig.Value.Authorize(AclAction.ViewComputeTasks, User))
			{
				return Forbid(AclAction.ViewComputeTasks);
			}

			IComputeClusterInfo clusterInfo;
			try
			{
				clusterInfo = await _computeService.GetClusterInfoAsync(clusterId);
			}
			catch (KeyNotFoundException)
			{
				return NotFound("Cluster '{ClusterId}' was found", clusterId);
			}

			GetComputeClusterInfo response = new GetComputeClusterInfo();
			response.Id = clusterInfo.Id;
			response.NamespaceId = clusterInfo.NamespaceId;
			response.RequestBucketId = clusterInfo.RequestBucketId;
			response.ResponseBucketId = clusterInfo.ResponseBucketId;

			return response;
		}

		/// <summary>
		/// Add tasks to be executed remotely
		/// </summary>
		/// <param name="clusterId">The cluster to add to</param>
		/// <param name="request">The request parameters</param>
		/// <returns></returns>
		[HttpPost]
		[Authorize]
		[Route("/api/v1/compute/{clusterId}")]
		public async Task<ActionResult> AddTasksAsync([FromRoute] ClusterId clusterId, [FromBody] AddTasksRequest request)
		{
			if(!_globalConfig.Value.Authorize(AclAction.AddComputeTasks, User))
			{
				return Forbid(AclAction.AddComputeTasks);
			}
			if (request.TaskRefIds.Count == 0)
			{
				return BadRequest("No task hashes specified");
			}

			await _computeService.AddTasksAsync(clusterId, request.ChannelId, request.TaskRefIds, request.RequirementsHash);
			return Ok();
		}

		/// <summary>
		/// Read updates for a particular channel
		/// </summary>
		/// <param name="clusterId"></param>
		/// <param name="channelId">The channel to add to</param>
		/// <param name="wait">Amount of time to wait before responding, in seconds</param>
		/// <returns></returns>
		[HttpPost]
		[Authorize]
		[Route("/api/v1/compute/{clusterId}/updates/{channelId}")]
		public async Task<ActionResult<GetTaskUpdatesResponse>> GetUpdatesAsync([FromRoute] ClusterId clusterId, [FromRoute] ChannelId channelId, [FromQuery] int wait = 0)
		{
			if (!_globalConfig.Value.Authorize(AclAction.ViewComputeTasks, User))
			{
				return Forbid(AclAction.ViewComputeTasks);
			}

			List<ComputeTaskStatus> updates;
			if (wait == 0)
			{
				updates = await _computeService.GetTaskUpdatesAsync(clusterId, channelId);
			}
			else
			{
				using CancellationTokenSource delaySource = new CancellationTokenSource(wait * 1000);
				updates = await _computeService.WaitForTaskUpdatesAsync(clusterId, channelId, delaySource.Token);
			}

			GetTaskUpdatesResponse response = new GetTaskUpdatesResponse();
			foreach (ComputeTaskStatus update in updates)
			{
				response.Updates.Add(update);
			}

			return response;
		}
	}
}
