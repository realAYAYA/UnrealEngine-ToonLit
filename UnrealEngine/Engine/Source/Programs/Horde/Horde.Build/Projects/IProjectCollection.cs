// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Build.Server;
using Horde.Build.Utilities;

namespace Horde.Build.Projects
{
	using ProjectId = StringId<IProject>;

	/// <summary>
	/// Interface for a collection of project documents
	/// </summary>
	public interface IProjectCollection
	{
		/// <summary>
		/// Updates the project configuration
		/// </summary>
		/// <param name="id">The project id</param>
		/// <param name="configPath">Path to the config file used to configure this project</param>
		/// <param name="revision">The config file revision</param>
		/// <param name="order">Order of the project</param>
		/// <param name="config">The configuration</param>
		/// <returns>New project instance</returns>
		Task<IProject?> AddOrUpdateAsync(ProjectId id, string configPath, string revision, int order, ProjectConfig config);

		/// <summary>
		/// Gets all the available projects
		/// </summary>
		/// <returns>List of project documents</returns>
		Task<List<IProject>> FindAllAsync();

		/// <summary>
		/// Gets a project by ID
		/// </summary>
		/// <param name="projectId">Unique id of the project</param>
		/// <returns>The project document</returns>
		Task<IProject?> GetAsync(ProjectId projectId);

		/// <summary>
		/// Gets the logo for a project
		/// </summary>
		/// <param name="projectId">The project id</param>
		/// <returns>The project logo document</returns>
		Task<IProjectLogo?> GetLogoAsync(ProjectId projectId);

		/// <summary>
		/// Sets the logo for a project
		/// </summary>
		/// <param name="projectId">The project id</param>
		/// <param name="path">Path to the source file</param>
		/// <param name="revision">Revision of the file</param>
		/// <param name="mimeType"></param>
		/// <param name="data"></param>
		/// <returns></returns>
		Task SetLogoAsync(ProjectId projectId, string path, string revision, string mimeType, byte[] data);

		/// <summary>
		/// Gets a project's permissions info by ID
		/// </summary>
		/// <param name="projectId">Unique id of the project</param>
		/// <returns>The project document</returns>
		Task<IProjectPermissions?> GetPermissionsAsync(ProjectId projectId);

		/// <summary>
		/// Deletes a project by id
		/// </summary>
		/// <param name="projectId">Unique id of the project</param>
		/// <returns>True if the project was deleted</returns>
		Task DeleteAsync(ProjectId projectId);
	}
}
