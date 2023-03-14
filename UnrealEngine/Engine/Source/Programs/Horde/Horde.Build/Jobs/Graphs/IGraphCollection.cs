// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Build.Jobs.Templates;

namespace Horde.Build.Jobs.Graphs
{
	/// <summary>
	/// Interface for a collection of graph documents
	/// </summary>
	public interface IGraphCollection
	{
		/// <summary>
		/// Adds a graph from a template
		/// </summary>
		/// <param name="template">The template</param>
		/// <returns>New graph</returns>
		Task<IGraph> AddAsync(ITemplate template);

		/// <summary>
		/// Creates a graph by appending groups and aggregates to an existing graph.
		/// </summary>
		/// <param name="baseGraph">The base graph</param>
		/// <param name="newGroupRequests">List of group requests</param>
		/// <param name="newAggregateRequests">List of aggregate requests</param>
		/// <param name="newLabelRequests">List of label requests</param>
		/// <returns>The new graph definition</returns>
		Task<IGraph> AppendAsync(IGraph? baseGraph, List<NewGroup>? newGroupRequests = null, List<NewAggregate>? newAggregateRequests = null, List<NewLabel>? newLabelRequests = null);

		/// <summary>
		/// Gets the graph for a job
		/// </summary>
		/// <param name="hash">Hash of the graph to retrieve</param>
		/// <returns>The graph for this job</returns>
		Task<IGraph> GetAsync(ContentHash hash);

		/// <summary>
		/// Finds all graphs stored in the collection
		/// </summary>
		/// <param name="hashes">Hashes to filter by</param>
		/// <param name="index">Starting index of the graph to return</param>
		/// <param name="count">Number of results to return</param>
		/// <returns>List of graphs</returns>
		Task<List<IGraph>> FindAllAsync(ContentHash[]? hashes = null, int? index = null, int? count = null);
	}
}
