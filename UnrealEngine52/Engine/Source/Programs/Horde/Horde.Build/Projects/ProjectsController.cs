// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Horde.Build.Acls;
using Horde.Build.Configuration;
using Horde.Build.Server;
using Horde.Build.Streams;
using Horde.Build.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.StaticFiles;
using Microsoft.Extensions.Options;

namespace Horde.Build.Projects
{
	using ProjectId = StringId<ProjectConfig>;

	/// <summary>
	/// Controller for the /api/v1/projects endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class ProjectsController : HordeControllerBase
	{
		private readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		private static readonly FileExtensionContentTypeProvider s_contentTypeProvider = new FileExtensionContentTypeProvider();

		/// <summary>
		/// Constructor
		/// </summary>
		public ProjectsController(IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_globalConfig = globalConfig;
		}

		/// <summary>
		/// Query all the projects
		/// </summary>
		/// <param name="includeStreams">Whether to include streams in the response</param>
		/// <param name="includeCategories">Whether to include categories in the response</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>Information about all the projects</returns>
		[HttpGet]
		[Route("/api/v1/projects")]
		[ProducesResponseType(typeof(List<GetProjectResponse>), 200)]
		public ActionResult<List<object>> GetProjects([FromQuery(Name = "Streams")] bool includeStreams = false, [FromQuery(Name = "Categories")] bool includeCategories = false, [FromQuery] PropertyFilter? filter = null)
		{
			GlobalConfig globalConfig = _globalConfig.Value;

			List<object> responses = new List<object>();
			foreach (ProjectConfig projectConfig in globalConfig.Projects)
			{
				if (projectConfig.Authorize(AclAction.ViewProject, User))
				{
					List<StreamConfig>? visibleStreams = null;
					if (includeStreams || includeCategories)
					{
						visibleStreams = projectConfig.Streams.Where(x => x.Authorize(AclAction.ViewStream, User)).ToList();
					}
					responses.Add(GetProjectResponse.FromConfig(projectConfig, includeStreams, includeCategories, visibleStreams).ApplyFilter(filter));
				}
			}
			return responses;
		}

		/// <summary>
		/// Retrieve information about a specific project
		/// </summary>
		/// <param name="projectId">Id of the project to get information about</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>Information about the requested project</returns>
		[HttpGet]
		[Route("/api/v1/projects/{projectId}")]
		[ProducesResponseType(typeof(List<GetProjectResponse>), 200)]
		public ActionResult<object> GetProject(ProjectId projectId, [FromQuery] PropertyFilter? filter = null)
		{
			ProjectConfig? projectConfig;
			if(!_globalConfig.Value.TryGetProject(projectId, out projectConfig))
			{
				return NotFound(projectId);
			}
			if (!projectConfig.Authorize(AclAction.ViewProject, User))
			{
				return Forbid(AclAction.ViewProject, projectId);
			}

			bool includeStreams = PropertyFilter.Includes(filter, nameof(GetProjectResponse.Streams));
			bool includeCategories = PropertyFilter.Includes(filter, nameof(GetProjectResponse.Categories));

			List<StreamConfig>? visibleStreams = null;
			if (includeStreams || includeCategories)
			{
				visibleStreams = projectConfig.Streams.Where(x => x.Authorize(AclAction.ViewStream, User)).ToList();
			}

			return GetProjectResponse.FromConfig(projectConfig, includeStreams, includeCategories, visibleStreams).ApplyFilter(filter);
		}

		/// <summary>
		/// Retrieve information about a specific project
		/// </summary>
		/// <param name="projectId">Id of the project to get information about</param>
		/// <returns>Information about the requested project</returns>
		[HttpGet]
		[Route("/api/v1/projects/{projectId}/logo")]
		public ActionResult<object> GetProjectLogo(ProjectId projectId)
		{
			ProjectConfig? projectConfig;
			if (!_globalConfig.Value.TryGetProject(projectId, out projectConfig))
			{
				return NotFound(projectId);
			}
			if (!projectConfig.Authorize(AclAction.ViewProject, User))
			{
				return Forbid(AclAction.ViewProject, projectId);
			}

			ConfigResource? logoResource = projectConfig.Logo;
			if (logoResource == null || logoResource.Path == null || logoResource.Data.Length == 0)
			{
				return NotFound("Missing logo resource data");
			}

			string? contentType;
			if (!s_contentTypeProvider.TryGetContentType(logoResource.Path, out contentType))
			{
				contentType = "application/octet-stream";
			}

			return new FileContentResult(logoResource.Data.ToArray(), contentType);
		}
	}
}
