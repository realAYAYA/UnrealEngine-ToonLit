// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using EpicGames.Core;
using Horde.Build.Acls;
using Horde.Build.Streams;
using Horde.Build.Utilities;

namespace Horde.Build.Projects
{
	using StreamId = StringId<IStream>;

	/// <summary>
	/// Stores configuration for a project
	/// </summary>
	[JsonSchema("https://unrealengine.com/horde/project")]
	[JsonSchemaCatalog("Horde Project", "Horde project configuration file", new[] { "*.project.json", "Projects/*.json" })]
	public class ProjectConfig
	{
		/// <summary>
		/// Name for the new project
		/// </summary>
		[Required]
		public string Name { get; set; } = null!;

		/// <summary>
		/// Path to the project logo
		/// </summary>
		public string? Logo { get; set; }

		/// <summary>
		/// Categories to include in this project
		/// </summary>
		public List<CreateProjectCategoryRequest> Categories { get; set; } = new List<CreateProjectCategoryRequest>();

		/// <summary>
		/// List of streams
		/// </summary>
		public List<StreamConfigRef> Streams { get; set; } = new List<StreamConfigRef>();

		/// <summary>
		/// Acl entries
		/// </summary>
		public UpdateAclRequest? Acl { get; set; }
	}

	/// <summary>
	/// Reference to configuration for a stream
	/// </summary>
	public class StreamConfigRef
	{
		/// <summary>
		/// Unique id for the stream
		/// </summary>
		[Required]
		public StreamId Id { get; set; } = StreamId.Empty;

		/// <summary>
		/// Path to the configuration file
		/// </summary>
		[Required]
		public string Path { get; set; } = null!;
	}
}
