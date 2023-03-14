// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Build.Acls;
using Horde.Build.Streams;
using Horde.Build.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;

namespace Horde.Build.Projects
{
	using ProjectId = StringId<IProject>;

	/// <summary>
	/// Controller for the /api/v1/projects endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class ProjectsController : HordeControllerBase
	{
		/// <summary>
		/// Singleton instance of the project service
		/// </summary>
		private readonly ProjectService _projectService;

		/// <summary>
		/// Singleton instance of the stream service
		/// </summary>
		private readonly StreamService _streamService;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="projectService">The project service</param>
		/// <param name="streamService">The stream service</param>
		public ProjectsController(ProjectService projectService, StreamService streamService)
		{
			_projectService = projectService;
			_streamService = streamService;
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
		public async Task<ActionResult<List<object>>> GetProjectsAsync([FromQuery(Name = "Streams")] bool includeStreams = false, [FromQuery(Name = "Categories")] bool includeCategories = false, [FromQuery] PropertyFilter? filter = null)
		{
			List<IProject> projects = await _projectService.GetProjectsAsync();
			ProjectPermissionsCache permissionsCache = new ProjectPermissionsCache();

			List<IStream>? streams = null;
			if (includeStreams || includeCategories)
			{
				streams = await _streamService.GetStreamsAsync();
			}

			List<object> responses = new List<object>();
			foreach (IProject project in projects)
			{
				if (await _projectService.AuthorizeAsync(project, AclAction.ViewProject, User, permissionsCache))
				{
					bool bIncludeAcl = await _projectService.AuthorizeAsync(project, AclAction.ViewPermissions, User, permissionsCache);
					responses.Add(project.ToResponse(includeStreams, includeCategories, streams, bIncludeAcl).ApplyFilter(filter));
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
		public async Task<ActionResult<object>> GetProjectAsync(ProjectId projectId, [FromQuery] PropertyFilter? filter = null)
		{
			IProject? project = await _projectService.GetProjectAsync(projectId);
			if (project == null)
			{
				return NotFound(projectId);
			}

			ProjectPermissionsCache cache = new ProjectPermissionsCache();
			if (!await _projectService.AuthorizeAsync(project, AclAction.ViewProject, User, cache))
			{
				return Forbid(AclAction.ViewProject, projectId);
			}

			bool bIncludeStreams = PropertyFilter.Includes(filter, nameof(GetProjectResponse.Streams));
			bool bIncludeCategories = PropertyFilter.Includes(filter, nameof(GetProjectResponse.Categories));

			List<IStream>? visibleStreams = null;
			if (bIncludeStreams || bIncludeCategories)
			{
				visibleStreams = new List<IStream>();

				List<IStream> streams = await _streamService.GetStreamsAsync(project.Id);
				foreach (IStream stream in streams)
				{
					if (await _streamService.AuthorizeAsync(stream, AclAction.ViewStream, User, cache))
					{
						visibleStreams.Add(stream);
					}
				}
			}

			bool bIncludeAcl = await _projectService.AuthorizeAsync(project, AclAction.ViewPermissions, User, cache);
			return project.ToResponse(bIncludeStreams, bIncludeCategories, visibleStreams, bIncludeAcl).ApplyFilter(filter);
		}

		/// <summary>
		/// Retrieve information about a specific project
		/// </summary>
		/// <param name="projectId">Id of the project to get information about</param>
		/// <returns>Information about the requested project</returns>
		[HttpGet]
		[Route("/api/v1/projects/{projectId}/logo")]
		public async Task<ActionResult<object>> GetProjectLogoAsync(ProjectId projectId)
		{
			IProject? project = await _projectService.GetProjectAsync(projectId);
			if (project == null)
			{
				return NotFound(projectId);
			}
			if (!await _projectService.AuthorizeAsync(project, AclAction.ViewProject, User, null))
			{
				return Forbid(AclAction.ViewProject, projectId);
			}

			IProjectLogo? projectLogo = await _projectService.Collection.GetLogoAsync(projectId);
			if (projectLogo == null)
			{
				return NotFound();
			}

			return new FileContentResult(projectLogo.Data, projectLogo.MimeType);
		}
	}
}
