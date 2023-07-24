// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Diagnostics;
using System.Security.Claims;
using System.Text.Json.Serialization;
using EpicGames.Core;
using Horde.Build.Acls;
using Horde.Build.Agents.Pools;
using Horde.Build.Configuration;
using Horde.Build.Server;
using Horde.Build.Streams;
using Horde.Build.Utilities;

namespace Horde.Build.Projects
{
	using ProjectId = StringId<ProjectConfig>;

	/// <summary>
	/// Stores configuration for a project
	/// </summary>
	[JsonSchema("https://unrealengine.com/horde/project")]
	[JsonSchemaCatalog("Horde Project", "Horde project configuration file", new[] { "*.project.json", "Projects/*.json" })]
	[ConfigIncludeRoot]
	[DebuggerDisplay("{Id}")]
	public class ProjectConfig
	{
		/// <summary>
		/// Accessor for the global config owning this project
		/// </summary>
		[JsonIgnore]
		public GlobalConfig GlobalConfig { get; private set; } = null!;

		/// <summary>
		/// The project id
		/// </summary>
		public ProjectId Id { get; set; }

		/// <summary>
		/// Name for the new project
		/// </summary>
		public string Name { get; set; } = null!;

		/// <summary>
		/// Direct include path for the project config. For backwards compatibility with old config files when including from a GlobalConfig object.
		/// </summary>
		[ConfigInclude, ConfigRelativePath]
		public string? Path { get; set; }

		/// <summary>
		/// Includes for other configuration files
		/// </summary>
		public List<ConfigInclude> Include { get; set; } = new List<ConfigInclude>();

		/// <summary>
		/// Order of this project on the dashboard
		/// </summary>
		public int Order { get; set; } = 128;

		/// <summary>
		/// Path to the project logo
		/// </summary>
		public ConfigResource? Logo { get; set; }

		/// <summary>
		/// List of pools for this project
		/// </summary>
		public List<PoolConfig> Pools { get; set; } = new List<PoolConfig>();

		/// <summary>
		/// Categories to include in this project
		/// </summary>
		public List<ProjectCategoryConfig> Categories { get; set; } = new List<ProjectCategoryConfig>();

		/// <summary>
		/// List of streams
		/// </summary>
		public List<StreamConfig> Streams { get; set; } = new List<StreamConfig>();

		/// <summary>
		/// Acl entries
		/// </summary>
		public AclConfig? Acl { get; set; }

		/// <summary>
		/// Callback after this configuration has been read
		/// </summary>
		/// <param name="id">Id of this project</param>
		/// <param name="globalConfig">The owning global config object</param>
		public void PostLoad(ProjectId id, GlobalConfig globalConfig)
		{
			Id = id;
			GlobalConfig = globalConfig;

			foreach (StreamConfig stream in Streams)
			{
				stream.PostLoad(stream.Id, this);
			}
		}

		/// <summary>
		/// Authorizes a user to perform a given action
		/// </summary>
		/// <param name="action">The action being performed</param>
		/// <param name="user">The principal to validate</param>
		public bool Authorize(AclAction action, ClaimsPrincipal user)
		{
			return Acl?.Authorize(action, user) ?? GlobalConfig.Authorize(action, user);
		}
	}

	/// <summary>
	/// Information about a category to display for a stream
	/// </summary>
	public class ProjectCategoryConfig
	{
		/// <summary>
		/// Name of this category
		/// </summary>
		[Required]
		public string Name { get; set; } = String.Empty;

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
	}
}
