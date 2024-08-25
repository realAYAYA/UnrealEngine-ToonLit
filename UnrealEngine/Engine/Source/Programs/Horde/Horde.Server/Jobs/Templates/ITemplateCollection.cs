// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading.Tasks;
using EpicGames.Core;

namespace Horde.Server.Jobs.Templates
{
	/// <summary>
	/// Interface for a collection of template documents
	/// </summary>
	public interface ITemplateCollection
	{
		/// <summary>
		/// Gets a template by ID
		/// </summary>
		/// <param name="templateId">Unique id of the template</param>
		/// <returns>The template document</returns>
		Task<ITemplate?> GetAsync(ContentHash templateId);

		/// <summary>
		/// Gets a template instance from its configuration
		/// </summary>
		/// <param name="templateConfig">The template configuration</param>
		/// <returns>Template instance</returns>
		Task<ITemplate> GetOrAddAsync(TemplateConfig templateConfig);
	}
}
