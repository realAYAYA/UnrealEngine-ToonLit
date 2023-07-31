// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using Horde.Build.Acls;
using Horde.Build.Projects;

namespace Horde.Build.Projects
{
	/// <summary>
	/// Information about a category to display for a stream
	/// </summary>
	public class CreateProjectCategoryRequest
	{
		/// <summary>
		/// Name of this category
		/// </summary>
		[Required]
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
		/// Constructor
		/// </summary>
		public CreateProjectCategoryRequest(string name)
		{
			Name = name;
		}
	}

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
		public GetProjectCategoryResponse(StreamCategory streamCategory)
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
		/// Custom permissions for this object
		/// </summary>
		public GetAclResponse? Acl { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="id">Unique id of the project</param>
		/// <param name="name">Name of the project</param>
		/// <param name="order">Order to show this project on the dashboard</param>
		/// <param name="streams">List of streams to display</param>
		/// <param name="categories">List of stream categories to display</param>
		/// <param name="acl">Custom permissions for this object</param>
		public GetProjectResponse(string id, string name, int order, List<GetProjectStreamResponse>? streams, List<GetProjectCategoryResponse>? categories, GetAclResponse? acl)
		{
			Id = id;
			Name = name;
			Order = order;
			Streams = streams;
			Categories = categories;
			Acl = acl;
		}
	}
}
