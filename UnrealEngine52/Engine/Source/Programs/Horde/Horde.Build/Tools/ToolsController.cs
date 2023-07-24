// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Storage;
using Horde.Build.Acls;
using Horde.Build.Server;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace Horde.Build.Tools
{
	using ToolId = StringId<ITool>;
	using ToolDeploymentId = ObjectId<IToolDeployment>;

	/// <summary>
	/// Describes a standalone, external tool hosted and deployed by Horde. Provides basic functionality for performing
	/// gradual roll-out, versioning, etc...
	/// </summary>
	public class GetToolResponse
	{
		readonly ITool _tool;

		/// <inheritdoc cref="VersionedDocument{TId, TLatest}.Id"/>
		public ToolId Id => _tool.Id;

		/// <inheritdoc cref="ToolConfig.Name"/>
		public string Name => _tool.Config.Name;

		/// <inheritdoc cref="ToolConfig.Description"/>
		public string Description => _tool.Config.Description;

		/// <inheritdoc cref="ITool.Deployments"/>
		public List<GetToolDeploymentResponse> Deployments { get; }

		/// <inheritdoc cref="ToolConfig.Public"/>
		public bool Public => _tool.Config.Public;

		/// <summary>
		/// Constructor
		/// </summary>
		public GetToolResponse(ITool tool)
		{
			_tool = tool;
			Deployments = tool.Deployments.ConvertAll(x => new GetToolDeploymentResponse(x));
		}
	}

	/// <summary>
	/// Response object describing the deployment of a tool
	/// </summary>
	public class GetToolDeploymentResponse
	{
		readonly IToolDeployment _deployment;

		/// <inheritdoc cref="IToolDeployment.Id"/>
		public ToolDeploymentId Id => _deployment.Id;

		/// <inheritdoc cref="IToolDeployment.Version"/>
		public string Version => _deployment.Version;

		/// <inheritdoc cref="IToolDeployment.State"/>
		public ToolDeploymentState State => _deployment.State;

		/// <inheritdoc cref="IToolDeployment.Progress"/>
		public double Progress => _deployment.Progress;

		/// <inheritdoc cref="IToolDeployment.StartedAt"/>
		public DateTime? StartedAt => _deployment.StartedAt;

		/// <inheritdoc cref="IToolDeployment.Duration"/>
		public TimeSpan Duration => _deployment.Duration;

		/// <inheritdoc cref="IToolDeployment.MimeType"/>
		public string MimeType => _deployment.MimeType;

		/// <inheritdoc cref="IToolDeployment.RefId"/>
		public RefId RefId => _deployment.RefId;

		/// <summary>
		/// Constructor
		/// </summary>
		public GetToolDeploymentResponse(IToolDeployment deployment) => _deployment = deployment;
	}

	/// <summary>
	/// Response from creating a deployment
	/// </summary>
	public class CreateDeploymentResponse
	{
		/// <summary>
		/// Identifier for the created deployment
		/// </summary>
		public ToolDeploymentId Id { get; set; }
	}

	/// <summary>
	/// Update an existing deployment
	/// </summary>
	public class UpdateDeploymentRequest
	{
		/// <summary>
		/// New state for the deployment
		/// </summary>
		public ToolDeploymentState? State { get; set; }
	}

	/// <summary>
	/// Action for a deployment
	/// </summary>
	public enum GetToolAction
	{
		/// <summary>
		/// Query for information about the deployment 
		/// </summary>
		Info,

		/// <summary>
		/// Download the deployment data
		/// </summary>
		Download,
	}

	/// <summary>
	/// Controller for the /api/v1/agents endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class ToolsController : HordeControllerBase
	{
		readonly ToolCollection _toolCollection;
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public ToolsController(ToolCollection toolCollection, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_toolCollection = toolCollection;
			_globalConfig = globalConfig;
		}

		/// <summary>
		/// Create a new deployment of the given tool.
		/// </summary>
		/// <returns>Information about the registered agent</returns>
		[HttpPost]
		[Route("/api/v1/tools/{id}/deployments")]
		public async Task<ActionResult<CreateDeploymentResponse>> CreateDeploymentAsync(ToolId id, [FromForm] ToolDeploymentConfig options, [FromForm] IFormFile file)
		{
			ITool? tool = await _toolCollection.GetAsync(id, _globalConfig.Value);
			if (tool == null)
			{
				return NotFound(id);
			}
			if (!tool.Config.Authorize(AclAction.UploadTool, User))
			{
				return Forbid(AclAction.UploadTool, id);
			}

			using (Stream stream = file.OpenReadStream())
			{
				tool = await _toolCollection.CreateDeploymentAsync(tool, options, stream, _globalConfig.Value);
				if (tool == null)
				{
					return NotFound(id);
				}
			}

			return new CreateDeploymentResponse { Id = tool.Deployments[^1].Id };
		}

		/// <summary>
		/// Register an agent to perform remote work.
		/// </summary>
		/// <returns>Information about the registered agent</returns>
		[HttpGet]
		[Route("/api/v1/tools/{id}/deployments/{deploymentId}")]
		public async Task<ActionResult> GetDeploymentAsync(ToolId id, ToolDeploymentId deploymentId, [FromQuery] GetToolAction action = GetToolAction.Info)
		{
			ITool? tool = await _toolCollection.GetAsync(id, _globalConfig.Value);
			if (tool == null)
			{
				return NotFound(id);
			}
			if (!tool.Config.Authorize(AclAction.DownloadTool, User))
			{
				return Forbid(AclAction.DownloadTool, id);
			}

			IToolDeployment? deployment = tool.Deployments.FirstOrDefault(x => x.Id == deploymentId);
			if(deployment == null)
			{
				return NotFound(id, deploymentId);
			}

			return await GetDeploymentResponseAsync(tool, deployment, action);
		}

		private async Task<ActionResult> GetDeploymentResponseAsync(ITool tool, IToolDeployment deployment, GetToolAction action)
		{
			if (action == GetToolAction.Info)
			{
				GetToolDeploymentResponse response = new GetToolDeploymentResponse(deployment);
				return Ok(response);
			}
			else
			{
				Stream stream = await _toolCollection.GetDeploymentPayloadAsync(tool, deployment);
				return new FileStreamResult(stream, "application/zip");
			}
		}

		/// <summary>
		/// Updates the state of an active deployment.
		/// </summary>
		[HttpPatch]
		[Route("/api/v1/tools/{id}/deployments/{deploymentId}")]
		public async Task<ActionResult> UpdateDeploymentAsync(ToolId id, ToolDeploymentId deploymentId, [FromBody] UpdateDeploymentRequest request)
		{
			ITool? tool = await _toolCollection.GetAsync(id, _globalConfig.Value);
			if (tool == null)
			{
				return NotFound(id);
			}
			if (!tool.Config.Authorize(AclAction.UploadTool, User))
			{
				return Forbid(AclAction.UploadTool, id);
			}

			if (request.State != null)
			{
				tool = await _toolCollection.UpdateDeploymentAsync(tool, deploymentId, request.State.Value);
				if (tool == null)
				{
					return NotFound(id, deploymentId);
				}
			}
			return Ok();
		}
	}

	/// <summary>
	/// Public methods available without authorization (or with very custom authorization)
	/// </summary>
	[ApiController]
	public class PublicToolsController : HordeControllerBase
	{
		readonly ToolCollection _toolCollection;
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;
		readonly IClock _clock;

		/// <summary>
		/// Constructor
		/// </summary>
		public PublicToolsController(ToolCollection toolCollection, IClock clock, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_toolCollection = toolCollection;
			_clock = clock;
			_globalConfig = globalConfig;
		}

		/// <summary>
		/// Register an agent to perform remote work.
		/// </summary>
		/// <returns>Information about the registered agent</returns>
		[HttpGet]
		[Route("/api/v1/tools/{id}")]
		public async Task<ActionResult> GetToolAsync(ToolId id, GetToolAction action = GetToolAction.Info)
		{
			ITool? tool = await _toolCollection.GetAsync(id, _globalConfig.Value);
			if (tool == null)
			{
				return NotFound(id);
			}
			if (!AuthorizeDownload(tool))
			{
				return Forbid(AclAction.DownloadTool, id);
			}

			if (action == GetToolAction.Info)
			{
				return Ok(tool);
			}
			else
			{
				if (tool.Deployments.Count == 0)
				{
					return NotFound(LogEvent.Create(LogLevel.Error, "Tool {ToolId} does not currently have any deployments", id));
				}

				return await GetDeploymentPayloadAsync(tool, tool.Deployments[^1]);
			}
		}

		/// <summary>
		/// Register an agent to perform remote work.
		/// </summary>
		/// <param name="id">The tool identifier</param>
		/// <param name="phase">Value indicating the client's preference for deployment to receive.</param>
		/// <param name="action">Information about the returned deployment</param>
		/// <returns>Information about the registered agent</returns>
		[HttpGet]
		[Route("/api/v1/tools/{id}/deployments")]
		public async Task<ActionResult> FindDeploymentAsync(ToolId id, [FromQuery] double phase = 0.0, [FromQuery] GetToolAction action = GetToolAction.Info)
		{
			ITool? tool = await _toolCollection.GetAsync(id, _globalConfig.Value);
			if (tool == null)
			{
				return NotFound(id);
			}
			if (!AuthorizeDownload(tool))
			{
				return Forbid(AclAction.DownloadTool, id);
			}
			if (tool.Deployments.Count == 0)
			{
				return NotFound(LogEvent.Create(LogLevel.Error, "Tool {ToolId} does not currently have any deployments", id));
			}

			DateTime utcNow = _clock.UtcNow;

			int idx = tool.Deployments.Count - 1;
			for (; idx > 0; idx--)
			{
				if (phase < tool.Deployments[idx].GetProgressValue(utcNow))
				{
					break;
				}
			}

			if (action == GetToolAction.Info)
			{
				return Ok(tool.Deployments[idx]);
			}
			else
			{
				return await GetDeploymentPayloadAsync(tool, tool.Deployments[idx]);
			}
		}

		async Task<ActionResult> GetDeploymentPayloadAsync(ITool tool, IToolDeployment deployment)
		{
			Stream stream = await _toolCollection.GetDeploymentPayloadAsync(tool, deployment);
			return new FileStreamResult(stream, deployment.MimeType);
		}

		bool AuthorizeDownload(ITool tool)
		{
			if (!tool.Config.Public)
			{
				if (User.Identity == null || !User.Identity.IsAuthenticated)
				{
					return false;
				}
				if (!tool.Config.Authorize(AclAction.DownloadTool, User))
				{
					return false;
				}
			}
			return true;
		}
	}
}
