// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Security.Claims;
using System.Threading.Tasks;
using Horde.Build.Acls;
using Horde.Build.Utilities;

namespace Horde.Build.Projects
{
	using ProjectId = StringId<IProject>;

	/// <summary>
	/// Cache of information about job ACLs
	/// </summary>
	public class ProjectPermissionsCache : GlobalPermissionsCache
	{
		/// <summary>
		/// Map of project id to permissions for that project
		/// </summary>
		public Dictionary<ProjectId, IProjectPermissions?> Projects { get; } = new Dictionary<ProjectId, IProjectPermissions?>();
	}

	/// <summary>
	/// Wraps functionality for manipulating projects
	/// </summary>
	public class ProjectService
	{
		/// <summary>
		/// The ACL service
		/// </summary>
		readonly AclService _aclService;

		/// <summary>
		/// Collection of project documents
		/// </summary>
		readonly IProjectCollection _projects;

		/// <summary>
		/// Accessor for the collection of project documents
		/// </summary>
		public IProjectCollection Collection => _projects;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="aclService">The ACL service</param>
		/// <param name="projects">Collection of project documents</param>
		public ProjectService(AclService aclService, IProjectCollection projects)
		{
			_aclService = aclService;
			_projects = projects;
		}

		/// <summary>
		/// Gets all the available projects
		/// </summary>
		/// <returns>List of project documents</returns>
		public Task<List<IProject>> GetProjectsAsync()
		{
			return _projects.FindAllAsync();
		}

		/// <summary>
		/// Gets a project by ID
		/// </summary>
		/// <param name="projectId">Unique id of the project</param>
		/// <returns>The project document</returns>
		public Task<IProject?> GetProjectAsync(ProjectId projectId)
		{
			return _projects.GetAsync(projectId);
		}

		/// <summary>
		/// Gets a project's permissions info by ID
		/// </summary>
		/// <param name="projectId">Unique id of the project</param>
		/// <returns>The project document</returns>
		public Task<IProjectPermissions?> GetProjectPermissionsAsync(ProjectId projectId)
		{
			return _projects.GetPermissionsAsync(projectId);
		}

		/// <summary>
		/// Deletes a project by id
		/// </summary>
		/// <param name="projectId">Unique id of the project</param>
		public async Task DeleteProjectAsync(ProjectId projectId)
		{
			await _projects.DeleteAsync(projectId);
		}

		/// <summary>
		/// Determines if the user is authorized to perform an action on a particular project
		/// </summary>
		/// <param name="acl">Acl for the project to check</param>
		/// <param name="action">The action being performed</param>
		/// <param name="user">The principal to authorize</param>
		/// <param name="cache">Cache for the scope table</param>
		/// <returns>True if the action is authorized</returns>
		private Task<bool> AuthorizeAsync(Acl? acl, AclAction action, ClaimsPrincipal user, GlobalPermissionsCache? cache)
		{
			bool? result = acl?.Authorize(action, user);
			if (result == null)
			{
				return _aclService.AuthorizeAsync(action, user, cache);
			}
			else
			{
				return Task.FromResult(result.Value);
			}
		}

		/// <summary>
		/// Determines if the user is authorized to perform an action on a particular project
		/// </summary>
		/// <param name="project">The project to check</param>
		/// <param name="action">The action being performed</param>
		/// <param name="user">The principal to authorize</param>
		/// <param name="cache">Cache for the scope table</param>
		/// <returns>True if the action is authorized</returns>
		public Task<bool> AuthorizeAsync(IProject project, AclAction action, ClaimsPrincipal user, GlobalPermissionsCache? cache)
		{
			return AuthorizeAsync(project.Acl, action, user, cache);
		}

		/// <summary>
		/// Determines if the user is authorized to perform an action on a particular project
		/// </summary>
		/// <param name="projectId">The project id to check</param>
		/// <param name="action">The action being performed</param>
		/// <param name="user">The principal to authorize</param>
		/// <param name="cache">Cache for project permissions</param>
		/// <returns>True if the action is authorized</returns>
		public async Task<bool> AuthorizeAsync(ProjectId projectId, AclAction action, ClaimsPrincipal user, ProjectPermissionsCache? cache)
		{
			IProjectPermissions? permissions;
			if (cache == null)
			{
				permissions = await GetProjectPermissionsAsync(projectId);
			}
			else if (!cache.Projects.TryGetValue(projectId, out permissions))
			{
				permissions = await GetProjectPermissionsAsync(projectId);
				cache.Projects.Add(projectId, permissions);
			}
			return await AuthorizeAsync(permissions?.Acl, action, user, cache);
		}
	}
}
