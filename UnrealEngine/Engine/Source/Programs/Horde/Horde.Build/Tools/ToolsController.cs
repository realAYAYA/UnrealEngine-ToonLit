// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using Horde.Build.Acls;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Logging;
using System;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace Horde.Build.Tools
{
	using ToolId = StringId<Tool>;
	using ToolDeploymentId = ObjectId<ToolDeployment>;

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
		readonly AclService _aclService;
		readonly ToolCollection _toolCollection;

		/// <summary>
		/// Constructor
		/// </summary>
		public ToolsController(AclService aclService, ToolCollection toolCollection)
		{
			_aclService = aclService;
			_toolCollection = toolCollection;
		}

		/// <summary>
		/// Create a new deployment of the given tool.
		/// </summary>
		/// <returns>Information about the registered agent</returns>
		[HttpPost]
		[Route("/api/v1/tools/{id}/deployments")]
		public async Task<ActionResult<CreateDeploymentResponse>> CreateDeploymentAsync(ToolId id, [FromForm] ToolDeploymentOptions options, [FromForm] IFormFile file)
		{
			Tool? tool = await _toolCollection.GetAsync(id);
			if (tool == null)
			{
				return NotFound(id);
			}
			if (!await tool.AuthorizeAsync(AclAction.UploadTool, User, _aclService, null))
			{
				return Forbid(AclAction.UploadTool, id);
			}

			using (Stream stream = file.OpenReadStream())
			{
				tool = await _toolCollection.CreateDeploymentAsync(tool, options, stream);
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
			Tool? tool = await _toolCollection.GetAsync(id);
			if (tool == null)
			{
				return NotFound(id);
			}
			if (!await tool.AuthorizeAsync(AclAction.DownloadTool, User, _aclService, null))
			{
				return Forbid(AclAction.DownloadTool, id);
			}

			ToolDeployment? deployment = tool.Deployments.FirstOrDefault(x => x.Id == deploymentId);
			if(deployment == null)
			{
				return NotFound(id, deploymentId);
			}

			return await GetDeploymentResponseAsync(tool, deployment, action);
		}

		private async Task<ActionResult> GetDeploymentResponseAsync(Tool tool, ToolDeployment deployment, GetToolAction action)
		{
			if (action == GetToolAction.Info)
			{
				return Ok(deployment);
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
			Tool? tool = await _toolCollection.GetAsync(id);
			if (tool == null)
			{
				return NotFound(id);
			}
			if (!await tool.AuthorizeAsync(AclAction.UploadTool, User, _aclService, null))
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
		readonly AclService _aclService;
		readonly ToolCollection _toolCollection;
		readonly IClock _clock;

		/// <summary>
		/// Constructor
		/// </summary>
		public PublicToolsController(AclService aclService, ToolCollection toolCollection, IClock clock)
		{
			_aclService = aclService;
			_toolCollection = toolCollection;
			_clock = clock;
		}

		/// <summary>
		/// Register an agent to perform remote work.
		/// </summary>
		/// <returns>Information about the registered agent</returns>
		[HttpGet]
		[Route("/api/v1/tools/{id}")]
		public async Task<ActionResult> GetToolAsync(ToolId id, GetToolAction action = GetToolAction.Info)
		{
			Tool? tool = await _toolCollection.GetAsync(id);
			if (tool == null)
			{
				return NotFound(id);
			}
			if (!await AuthorizeDownloadAsync(tool))
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
			Tool? tool = await _toolCollection.GetAsync(id);
			if (tool == null)
			{
				return NotFound(id);
			}
			if (!await AuthorizeDownloadAsync(tool))
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

		async Task<ActionResult> GetDeploymentPayloadAsync(Tool tool, ToolDeployment deployment)
		{
			Stream stream = await _toolCollection.GetDeploymentPayloadAsync(tool, deployment);
			return new FileStreamResult(stream, deployment.MimeType);
		}

		async ValueTask<bool> AuthorizeDownloadAsync(Tool tool)
		{
			if (!tool.Public)
			{
				if (User.Identity == null || !User.Identity.IsAuthenticated)
				{
					return false;
				}
				if (!await tool.AuthorizeAsync(AclAction.DownloadTool, User, _aclService, null))
				{
					return false;
				}
			}
			return true;
		}
	}
}
