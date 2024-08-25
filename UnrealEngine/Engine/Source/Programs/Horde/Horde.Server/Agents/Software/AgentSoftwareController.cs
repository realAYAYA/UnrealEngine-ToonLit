// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Horde.Server.Server;
using Horde.Server.Tools;
using HordeCommon;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;
using Microsoft.Net.Http.Headers;

namespace Horde.Server.Agents.Software
{
	/// <summary>
	/// Information about an agent software channel
	/// </summary>
	public class GetAgentSoftwareChannelResponse
	{
		/// <summary>
		/// Version number of this software
		/// </summary>
		public string? Version { get; set; }
	}

	/// <summary>
	/// Controller for the /api/v1/agentsoftware endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class AgentSoftwareController : ControllerBase
	{
		private readonly IToolCollection _toolCollection;
		private readonly IClock _clock;
		private readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public AgentSoftwareController(IToolCollection toolCollection, IClock clock, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_toolCollection = toolCollection;
			_clock = clock;
			_globalConfig = globalConfig;
		}

		/// <summary>
		/// Finds all uploaded software matching the given criteria
		/// </summary>
		/// <returns>Http response</returns>
		[HttpGet]
		[Obsolete("Agent software is now stored as a tool. This endpoint exists for backwards compatibility, but will be removed in the future.")]
		[Route("/api/v1/agentsoftware/default")]
		[ProducesResponseType(typeof(GetAgentSoftwareChannelResponse), 200)]
		public async Task<ActionResult<object>> FindSoftwareAsync()
		{
			if (!_globalConfig.Value.Authorize(AgentSoftwareAclAction.DownloadSoftware, User))
			{
				return Forbid();
			}

			ITool? tool = await _toolCollection.GetAsync(AgentExtensions.AgentToolId, _globalConfig.Value);
			if (tool == null)
			{
				return NotFound("No agent software tool is currently registered");
			}

			IToolDeployment? deployment = tool.GetCurrentDeployment(1.0, _clock.UtcNow);
			if (deployment == null)
			{
				return NotFound("No deployment currently set for agent software");
			}

			return new GetAgentSoftwareChannelResponse { Version = deployment.Version };
		}

		/// <summary>
		/// Gets the zip file for a specific channel
		/// </summary>
		/// <param name="cancellationToken">Cancellation token</param>
		/// <returns>Http response</returns>
		[HttpGet]
		[Obsolete("Agent software is now stored as a tool. This endpoint exists for backwards compatibility, but will be removed in the future.")]
		[Route("/api/v1/agentsoftware/default/zip")]
		public async Task<ActionResult> GetArchiveAsync(CancellationToken cancellationToken)
		{
			if (!_globalConfig.Value.Authorize(AgentSoftwareAclAction.DownloadSoftware, User))
			{
				return Forbid();
			}

			ITool? tool = await _toolCollection.GetAsync(AgentExtensions.AgentToolId, _globalConfig.Value, cancellationToken);
			if (tool == null)
			{
				return NotFound("No agent software tool is currently registered");
			}

			IToolDeployment? deployment = tool.GetCurrentDeployment(1.0, _clock.UtcNow);
			if (deployment == null)
			{
				return NotFound("No deployment currently set for agent software");
			}

			Stream stream = await _toolCollection.GetDeploymentZipAsync(tool, deployment, cancellationToken);
			return new FileStreamResult(stream, new MediaTypeHeaderValue("application/octet-stream")) { FileDownloadName = $"HordeAgent.zip" };
		}
	}
}
