// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Nodes;
using Horde.Server.Acls;
using Horde.Server.Server;
using Horde.Server.Storage;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.StaticFiles;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Horde.Server.Tools
{
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
		public GetToolResponse(ITool tool, List<GetToolDeploymentResponse> deployments)
		{
			_tool = tool;
			Deployments = deployments;
		}
	}

	/// <summary>
	/// Summary for a particular tool.
	/// </summary>
	public class GetToolSummaryResponse
	{
		readonly ITool _tool;

		/// <inheritdoc cref="VersionedDocument{TId, TLatest}.Id"/>
		public ToolId Id => _tool.Id;

		/// <inheritdoc cref="ToolConfig.Name"/>
		public string Name => _tool.Config.Name;

		/// <inheritdoc cref="ToolConfig.Description"/>
		public string Description => _tool.Config.Description;

		/// <inheritdoc cref="IToolDeployment.Version"/>
		public string? Version => (_tool.Deployments.Count > 0) ? _tool.Deployments[^1].Version : null;

		/// <summary>
		/// Constructor
		/// </summary>
		internal GetToolSummaryResponse(ITool tool) => _tool = tool;
	}

	/// <summary>
	/// Response when querying all tools
	/// </summary>
	public class GetToolsSummaryResponse
	{
		/// <summary>
		/// List of tools.
		/// </summary>
		public List<GetToolSummaryResponse> Tools { get; } = new List<GetToolSummaryResponse>();
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

		/// <inheritdoc cref="IToolDeployment.RefName"/>
		public RefName RefName => _deployment.RefName;

		/// <summary>
		/// Hash of the root node
		/// </summary>
		public IoHash Hash { get; }

		/// <summary>
		/// Node for downloading this deployment
		/// </summary>
		public NodeLocator Locator { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public GetToolDeploymentResponse(IToolDeployment deployment, BlobHandle handle)
		{
			_deployment = deployment;
			Hash = handle.Hash;
			Locator = handle.GetLocator();
		}
	}

	/// <summary>
	/// Request for creating a new deployment
	/// </summary>
	public class CreateDeploymentRequest
	{
		/// <inheritdoc cref="IToolDeployment.Version"/>
		public string Version { get; set; } = "Unknown";

		/// <summary>
		/// Number of minutes over which to do the deployment
		/// </summary>
		public double? Duration { get; set; }

		/// <summary>
		/// Whether to create the deployment in a paused state
		/// </summary>
		public bool? CreatePaused { get; set; }

		/// <summary>
		/// Handle to the root node
		/// </summary>
		public string Node { get; set; } = null!;
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

		/// <summary>
		/// Download the deployment data as a zip file
		/// </summary>
		Zip,
	}

	/// <summary>
	/// Controller for the /api/v1/agents endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class ToolsController : HordeControllerBase
	{
		readonly IToolCollection _toolCollection;
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public ToolsController(IToolCollection toolCollection, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_toolCollection = toolCollection;
			_globalConfig = globalConfig;
		}

		/// <summary>
		/// Uploads blob data for a new tool deployment.
		/// </summary>
		/// <param name="id">Identifier of the tool to upload</param>
		/// <param name="file">Data to be uploaded. May be null, in which case the server may return a separate url.</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		[HttpPost]
		[Route("/api/v1/tools/{id}/blobs")]
		public async Task<ActionResult<WriteBlobResponse>> WriteBlobAsync(ToolId id, IFormFile? file, CancellationToken cancellationToken = default)
		{
			ITool? tool = await _toolCollection.GetAsync(id, _globalConfig.Value);
			if (tool == null)
			{
				return NotFound(id);
			}
			if (!tool.Config.Authorize(ToolAclAction.UploadTool, User))
			{
				return Forbid(ToolAclAction.UploadTool, id);
			}

			IStorageClient storageClient = await _toolCollection.GetStorageClientAsync(tool, cancellationToken);
			return await StorageController.WriteBlobAsync(storageClient, file, cancellationToken: cancellationToken);
		}

		/// <summary>
		/// Create a new deployment of the given tool.
		/// </summary>
		/// <returns>Information about the registered agent</returns>
		[HttpPost]
		[Route("/api/v1/tools/{id}/deployments")]
		public async Task<ActionResult<CreateDeploymentResponse>> CreateDeploymentAsync(ToolId id, [FromForm] ToolDeploymentConfig options, [FromForm] IFormFile file, CancellationToken cancellationToken)
		{
			ITool? tool = await _toolCollection.GetAsync(id, _globalConfig.Value);

			if (tool == null)
			{
				return NotFound(id);
			}
			if (!tool.Config.Authorize(ToolAclAction.UploadTool, User))
			{
				return Forbid(ToolAclAction.UploadTool, id);
			}

			using (Stream stream = file.OpenReadStream())
			{
				tool = await _toolCollection.CreateDeploymentAsync(tool, options, stream, _globalConfig.Value, cancellationToken);
				if (tool == null)
				{
					return NotFound(id);
				}
			}

			return new CreateDeploymentResponse { Id = tool.Deployments[^1].Id };
		}

		/// <summary>
		/// Create a new deployment of the given tool.
		/// </summary>
		[HttpPost]
		[Route("/api/v2/tools/{id}/deployments")]
		public async Task<ActionResult<CreateDeploymentResponse>> CreateDeploymentAsync(ToolId id, CreateDeploymentRequest request, CancellationToken cancellationToken)
		{
			ITool? tool = await _toolCollection.GetAsync(id, _globalConfig.Value);

			if (tool == null)
			{
				return NotFound(id);
			}
			if (!tool.Config.Authorize(ToolAclAction.UploadTool, User))
			{
				return Forbid(ToolAclAction.UploadTool, id);
			}

			ToolDeploymentConfig options = new ToolDeploymentConfig { Version = request.Version, Duration = TimeSpan.FromMinutes(request.Duration ?? 0.0), CreatePaused = request.CreatePaused ?? false };

			tool = await _toolCollection.CreateDeploymentAsync(tool, options, NodeLocator.Parse(request.Node), _globalConfig.Value, cancellationToken);
			if (tool == null)
			{
				return NotFound(id);
			}

			return new CreateDeploymentResponse { Id = tool.Deployments[^1].Id };
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
			if (!tool.Config.Authorize(ToolAclAction.UploadTool, User))
			{
				return Forbid(ToolAclAction.UploadTool, id);
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
		readonly IToolCollection _toolCollection;
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;
		readonly IClock _clock;
		readonly IMemoryCache _cache;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public PublicToolsController(IToolCollection toolCollection, IClock clock, IOptionsSnapshot<GlobalConfig> globalConfig, IMemoryCache cache, ILogger<ToolsController> logger)
		{
			_toolCollection = toolCollection;
			_clock = clock;
			_globalConfig = globalConfig;
			_cache = cache;
			_logger = logger;
		}

		/// <summary>
		/// Create a new deployment of the given tool.
		/// </summary>
		/// <returns>Information about the registered agent</returns>
		[HttpGet]
		[Route("/api/v1/tools")]
		public async Task<ActionResult<GetToolsSummaryResponse>> GetToolsAsync()
		{
			GlobalConfig globalConfig = _globalConfig.Value;

			Dictionary<ToolId, ToolConfig> tools = new Dictionary<ToolId, ToolConfig>();
			foreach (BundledToolConfig bundledToolConfig in globalConfig.ServerSettings.BundledTools)
			{
				tools[bundledToolConfig.Id] = bundledToolConfig;
			}
			foreach (ToolConfig toolConfig in globalConfig.Tools)
			{
				tools[toolConfig.Id] = toolConfig;
			}

			GetToolsSummaryResponse response = new GetToolsSummaryResponse();
			foreach (ToolConfig toolConfig in tools.Values.OrderBy(x => x.Name, StringComparer.Ordinal))
			{
				if(AuthorizeDownload(toolConfig))
				{
					ITool? tool = await _toolCollection.GetAsync(toolConfig.Id, _globalConfig.Value);
					if (tool != null)
					{
						response.Tools.Add(new GetToolSummaryResponse(tool));
					}
				}
			}

			return response;
		}

		/// <summary>
		/// Gets information about a particular tool
		/// </summary>
		/// <returns>Information about the registered agent</returns>
		[HttpGet]
		[Route("/api/v1/tools/{id}")]
		public async Task<ActionResult> GetToolAsync(ToolId id, GetToolAction action = GetToolAction.Info, CancellationToken cancellationToken = default)
		{
			ITool? tool = await _toolCollection.GetAsync(id, _globalConfig.Value);
			if (tool == null)
			{
				return NotFound(id);
			}
			if (!AuthorizeDownload(tool.Config))
			{
				return Forbid(ToolAclAction.DownloadTool, id);
			}

			if (action == GetToolAction.Info)
			{
				List<GetToolDeploymentResponse> deploymentResponses = new List<GetToolDeploymentResponse>();
				foreach (IToolDeployment deployment in tool.Deployments)
				{
					GetToolDeploymentResponse deploymentResponse = await GetDeploymentInfoResponseAsync(tool, deployment, cancellationToken);
					deploymentResponses.Add(deploymentResponse);
				}
				return Ok(new GetToolResponse(tool, deploymentResponses));
			}
			else
			{
				if (tool.Deployments.Count == 0)
				{
					return NotFound(LogEvent.Create(LogLevel.Error, "Tool {ToolId} does not currently have any deployments", id));
				}

				return await GetDeploymentResponseAsync(tool, tool.Deployments[^1], action, cancellationToken);
			}
		}

		/// <summary>
		/// Finds deployments of a particular tool.
		/// </summary>
		/// <param name="id">The tool identifier</param>
		/// <param name="phase">Value indicating the client's preference for deployment to receive.</param>
		/// <param name="action">Information about the returned deployment</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about the registered agent</returns>
		[HttpGet]
		[Route("/api/v1/tools/{id}/deployments")]
		public async Task<ActionResult> FindDeploymentAsync(ToolId id, [FromQuery] double phase = 0.0, [FromQuery] GetToolAction action = GetToolAction.Info, CancellationToken cancellationToken = default)
		{
			ITool? tool = await _toolCollection.GetAsync(id, _globalConfig.Value);
			if (tool == null)
			{
				return NotFound(id);
			}
			if (!AuthorizeDownload(tool.Config))
			{
				return Forbid(ToolAclAction.DownloadTool, id);
			}

			IToolDeployment? deployment = tool.GetCurrentDeployment(phase, _clock.UtcNow);
			if (deployment == null)
			{
				return NotFound(LogEvent.Create(LogLevel.Error, "Tool {ToolId} does not currently have any deployments", id));
			}

			return await GetDeploymentResponseAsync(tool, deployment, action, cancellationToken);
		}

		/// <summary>
		/// Gets information about a specific tool deployment.
		/// </summary>
		/// <returns>Information about the registered agent</returns>
		[HttpGet]
		[Route("/api/v1/tools/{id}/deployments/{deploymentId}")]
		public async Task<ActionResult> GetDeploymentAsync(ToolId id, ToolDeploymentId deploymentId, [FromQuery] GetToolAction action = GetToolAction.Info, CancellationToken cancellationToken = default)
		{
			ITool? tool = await _toolCollection.GetAsync(id, _globalConfig.Value);
			if (tool == null)
			{
				return NotFound(id);
			}
			if (!AuthorizeDownload(tool.Config))
			{
				return Forbid(ToolAclAction.DownloadTool, id);
			}

			IToolDeployment? deployment = tool.Deployments.FirstOrDefault(x => x.Id == deploymentId);
			if (deployment == null)
			{
				return NotFound(id, deploymentId);
			}

			return await GetDeploymentResponseAsync(tool, deployment, action, cancellationToken);
		}

		private async Task<ActionResult> GetDeploymentResponseAsync(ITool tool, IToolDeployment deployment, GetToolAction action, CancellationToken cancellationToken)
		{
			if (action == GetToolAction.Info)
			{
				GetToolDeploymentResponse response = await GetDeploymentInfoResponseAsync(tool, deployment, cancellationToken);
				return Ok(response);
			}

			IStorageClient client = await _toolCollection.GetStorageClientAsync(tool, cancellationToken);

			DirectoryNode node = await client.ReadNodeAsync<DirectoryNode>(deployment.RefName, DateTime.UtcNow - TimeSpan.FromDays(2.0), cancellationToken);

			if (node.Directories.Count == 0 && node.Files.Count == 1 && action != GetToolAction.Zip)
			{
				FileEntry entry = node.Files.First();

				string? contentType;
				if (!new FileExtensionContentTypeProvider().TryGetContentType(entry.Name.ToString(), out contentType))
				{
					contentType = "application/octet-stream";
				}

				return new FileStreamResult(entry.AsStream(), contentType) { FileDownloadName = entry.Name.ToString() };
			}

			Stream stream = node.AsZipStream();
			return new FileStreamResult(stream, "application/zip") { FileDownloadName = $"{tool.Id}-{deployment.Version}.zip" };
		}

		private async Task<GetToolDeploymentResponse> GetDeploymentInfoResponseAsync(ITool tool, IToolDeployment deployment, CancellationToken cancellationToken)
		{
			IStorageClient client = await _toolCollection.GetStorageClientAsync(tool, cancellationToken);
			BlobHandle rootHandle = await client.ReadRefTargetAsync(deployment.RefName, cancellationToken: cancellationToken);

			return new GetToolDeploymentResponse(deployment, rootHandle);
		}

		/// <summary>
		/// Retrieves blobs for a particular tool
		/// </summary>
		/// <param name="id">Identifier of the tool to retrieve</param>
		/// <param name="locator">The blob locator</param>
		/// <param name="length">Length of data to return</param>
		/// <param name="offset">Offset of the data to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about all the artifacts</returns>
		[HttpGet]
		[Route("/api/v1/tools/{id}/blobs/{*locator}")]
		public async Task<ActionResult<object>> ReadToolBlobAsync(ToolId id, BlobLocator locator, [FromQuery] int? offset = null, [FromQuery] int? length = null, CancellationToken cancellationToken = default)
		{
			ITool? tool = await _toolCollection.GetAsync(id, _globalConfig.Value);
			if (tool == null)
			{
				return NotFound(id);
			}
			if (!AuthorizeDownload(tool.Config))
			{
				return Forbid(ToolAclAction.DownloadTool, id);
			}

			if (!locator.BlobId.WithinFolder(tool.Id.Id.Text))
			{
				return BadRequest("Invalid blob id for tool");
			}

			IStorageClient storageClient = await _toolCollection.GetStorageClientAsync(tool, cancellationToken);
			return StorageController.ReadBlobInternalAsync(storageClient, locator, offset, length, cancellationToken);
		}

		bool AuthorizeDownload(ToolConfig toolConfig)
		{
			if (!toolConfig.Public)
			{
				if (User.Identity == null || !User.Identity.IsAuthenticated)
				{
					return false;
				}
				if (!toolConfig.Authorize(ToolAclAction.DownloadTool, User))
				{
					return false;
				}
			}
			return true;
		}
	}
}
