// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Linq;
using System.Text.RegularExpressions;
using Horde.Build.Acls;
using Horde.Build.Streams;

namespace Horde.Build.Projects
{
	/// <summary>
	/// Information about a stream within a project
	/// </summary>
	public class GetProjectStreamResponse
	{
		/// <summary>
		/// The stream id
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// The stream name
		/// </summary>
		public string Name { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="id">The unique stream id</param>
		/// <param name="name">The stream name</param>
		public GetProjectStreamResponse(string id, string name)
		{
			Id = id;
			Name = name;
		}
	}

	/// <summary>
	/// Information about a category to display for a stream
	/// </summary>
	public class GetProjectCategoryResponse
	{
		/// <summary>
		/// Heading for this column
		/// </summary>
		public string Name { get; set; }

		/// <summary>
		/// Index of the row to display this category on
		/// </summary>
		public int Row { get; set; }

		/// <summary>
		/// Whether to show this category on the nav menu
		/// </summary>
		public bool ShowOnNavMenu { get; set; }

		/// <summary>
		/// Patterns for stream names to include
		/// </summary>
		public List<string> IncludePatterns { get; set; } = new List<string>();

		/// <summary>
		/// Patterns for stream names to exclude
		/// </summary>
		public List<string> ExcludePatterns { get; set; } = new List<string>();

		/// <summary>
		/// Streams to include in this category
		/// </summary>
		public List<string> Streams { get; set; } = new List<string>();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="streamCategory">The category to construct from</param>
		public GetProjectCategoryResponse(ProjectCategoryConfig streamCategory)
		{
			Name = streamCategory.Name;
			Row = streamCategory.Row;
			ShowOnNavMenu = streamCategory.ShowOnNavMenu;
			IncludePatterns = streamCategory.IncludePatterns;
			ExcludePatterns = streamCategory.ExcludePatterns;
		}
	}

	/// <summary>
	/// Response describing a project
	/// </summary>
	public class GetProjectResponse
	{
		/// <summary>
		/// Unique id of the project
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// Name of the project
		/// </summary>
		public string Name { get; set; }

		/// <summary>
		/// Order to display this project on the dashboard
		/// </summary>
		public int Order { get; set; }

		/// <summary>
		/// List of streams that are in this project
		/// </summary>
		public List<GetProjectStreamResponse>? Streams { get; set; }

		/// <summary>
		/// List of stream categories to display
		/// </summary>
		public List<GetProjectCategoryResponse>? Categories { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="id">Unique id of the project</param>
		/// <param name="name">Name of the project</param>
		/// <param name="order">Order to show this project on the dashboard</param>
		/// <param name="streams">List of streams to display</param>
		/// <param name="categories">List of stream categories to display</param>
		public GetProjectResponse(string id, string name, int order, List<GetProjectStreamResponse>? streams, List<GetProjectCategoryResponse>? categories)
		{
			Id = id;
			Name = name;
			Order = order;
			Streams = streams;
			Categories = categories;
		}

		/// <summary>
		/// Converts this object to a public response
		/// </summary>
		/// <param name="projectConfig">The project instance</param>
		/// <param name="includeStreams">Whether to include streams in the response</param>
		/// <param name="includeCategories">Whether to include categories in the response</param>
		/// <param name="streamConfigs">The list of streams</param>
		/// <returns>Response instance</returns>
		public static GetProjectResponse FromConfig(ProjectConfig projectConfig, bool includeStreams, bool includeCategories, List<StreamConfig>? streamConfigs)
		{
			List<GetProjectStreamResponse>? streamResponses = null;
			if (includeStreams)
			{
				streamResponses = streamConfigs!.ConvertAll(x => new GetProjectStreamResponse(x.Id.ToString(), x.Name));
			}

			List<GetProjectCategoryResponse>? categoryResponses = null;
			if (includeCategories)
			{
				categoryResponses = projectConfig.Categories.ConvertAll(x => new GetProjectCategoryResponse(x));
				if (streamConfigs != null)
				{
					foreach (StreamConfig streamConfig in streamConfigs)
					{
						GetProjectCategoryResponse? categoryResponse = categoryResponses.FirstOrDefault(x => MatchCategory(streamConfig.Name, x));
						if (categoryResponse == null)
						{
							int row = (categoryResponses.Count > 0) ? categoryResponses.Max(x => x.Row) : 0;
							if (categoryResponses.Count(x => x.Row == row) >= 3)
							{
								row++;
							}

							ProjectCategoryConfig otherCategory = new ProjectCategoryConfig();
							otherCategory.Name = "Other";
							otherCategory.Row = row;
							otherCategory.IncludePatterns.Add(".*");

							categoryResponse = new GetProjectCategoryResponse(otherCategory);
							categoryResponses.Add(categoryResponse);
						}
						categoryResponse.Streams!.Add(streamConfig.Id.ToString());
					}
				}
			}

			return new GetProjectResponse(projectConfig.Id.ToString(), projectConfig.Name, projectConfig.Order, streamResponses, categoryResponses);
		}

		/// <summary>
		/// Tests if a category response matches a given stream name
		/// </summary>
		/// <param name="name">The stream name</param>
		/// <param name="category">The category response</param>
		/// <returns>True if the category matches</returns>
		static bool MatchCategory(string name, GetProjectCategoryResponse category)
		{
			if (category.IncludePatterns.Any(x => Regex.IsMatch(name, x)))
			{
				if (!category.ExcludePatterns.Any(x => Regex.IsMatch(name, x)))
				{
					return true;
				}
			}
			return false;
		}
	}
}
