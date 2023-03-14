// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Core;
using HordeCommon;

namespace Horde.Build.Jobs.Templates
{
	/// <summary>
	/// Interface for a collection of template documents
	/// </summary>
	public interface ITemplateCollection
	{
		/// <summary>
		/// Public constructor
		/// </summary>
		/// <param name="name">Name of the template</param>
		/// <param name="priority">Priority of this template</param>
		/// <param name="bAllowPreflights">Whether to allow preflights of this job</param>
		/// <param name="bUpdateIssues"> Whether to update issues for all jobs using this template</param>
		/// <param name="bPromoteIssuesByDefault">Whether to promote issues by default for all jobs using this template</param>
		/// <param name="initialAgentType">The agent type to parse the buildgraph script</param>
		/// <param name="submitNewChange">Path to a file within the stream to submit to generate a new changelist for jobs</param>
		/// <param name="submitDescription">Description for new changes submitted to the stream</param>
		/// <param name="arguments">List of arguments which are always specified</param>
		/// <param name="parameters">List of template parameters</param>
		Task<ITemplate> AddAsync(string name, Priority? priority = null, bool bAllowPreflights = true, bool bUpdateIssues = false, bool bPromoteIssuesByDefault = false, string? initialAgentType = null, string? submitNewChange = null, string? submitDescription = null, List<string>? arguments = null, List<Parameter>? parameters = null);

		/// <summary>
		/// Gets all the available templates
		/// </summary>
		/// <returns>List of template documents</returns>
		Task<List<ITemplate>> FindAllAsync();

		/// <summary>
		/// Gets a template by ID
		/// </summary>
		/// <param name="templateId">Unique id of the template</param>
		/// <returns>The template document</returns>
		Task<ITemplate?> GetAsync(ContentHash templateId);
	}
}
