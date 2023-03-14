// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using EpicGames.Core;
using EpicGames.Horde.Common;
using EpicGames.Serialization;
using Horde.Build.Acls;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Projects;
using Horde.Build.Streams;
using Horde.Build.Tools;
using Horde.Build.Utilities;

namespace Horde.Build.Server
{
	using ProjectId = StringId<IProject>;
	using StreamId = StringId<IStream>;
	using TemplateRefId = StringId<TemplateRef>;
	
	/// <summary>
	/// Global configuration
	/// </summary>
	[JsonSchema("https://unrealengine.com/horde/global")]
	[JsonSchemaCatalog("Horde Globals", "Horde global configuration file", "globals.json")]
	public class GlobalConfig
	{
		/// <summary>
		/// List of projects
		/// </summary>
		public List<ProjectConfigRef> Projects { get; set; } = new List<ProjectConfigRef>();

		/// <summary>
		/// List of scheduled downtime
		/// </summary>
		public List<ScheduledDowntime> Downtime { get; set; } = new List<ScheduledDowntime>();

		/// <summary>
		/// List of Perforce clusters
		/// </summary>
		public List<PerforceCluster> PerforceClusters { get; set; } = new List<PerforceCluster>();

		/// <summary>
		/// List of costs of a particular agent type
		/// </summary>
		public List<AgentRateConfig> Rates { get; set; } = new List<AgentRateConfig>();

		/// <summary>
		/// List of compute profiles
		/// </summary>
		public List<ComputeClusterConfig> Compute { get; set; } = new List<ComputeClusterConfig>();

		/// <summary>
		/// List of tools hosted by the server
		/// </summary>
		public List<ToolOptions> Tools { get; set; } = new List<ToolOptions>();

		/// <summary>
		/// Maximum number of conforms to run at once
		/// </summary>
		public int MaxConformCount { get; set; }

		/// <summary>
		/// List of storage namespaces
		/// </summary>
		public StorageConfig? Storage { get; set; }

		/// <summary>
		/// Access control list
		/// </summary>
		public UpdateAclRequest? Acl { get; set; }
	}

	/// <summary>
	/// Profile for executing compute requests
	/// </summary>
	public class ComputeClusterConfig
	{
		/// <summary>
		/// Name of the partition
		/// </summary>
		public string Id { get; set; } = "default";

		/// <summary>
		/// Name of the namespace to use
		/// </summary>
		public string NamespaceId { get; set; } = "horde.compute";

		/// <summary>
		/// Name of the input bucket
		/// </summary>
		public string RequestBucketId { get; set; } = "requests";

		/// <summary>
		/// Name of the output bucket
		/// </summary>
		public string ResponseBucketId { get; set; } = "responses";

		/// <summary>
		/// Access control list
		/// </summary>
		public UpdateAclRequest? Acl { get; set; }
	}

	/// <summary>
	/// References a project configuration
	/// </summary>
	public class ProjectConfigRef
	{
		/// <summary>
		/// Unique id for the project
		/// </summary>
		[Required]
		public ProjectId Id { get; set; } = ProjectId.Empty;

		/// <summary>
		/// Config path for the project
		/// </summary>
		[Required]
		public string Path { get; set; } = null!;
	}

	/// <summary>
	/// Configuration for storage system
	/// </summary>
	public class StorageConfig
	{
		/// <summary>
		/// List of storage namespaces
		/// </summary>
		public List<NamespaceConfig> Namespaces { get; set; } = new List<NamespaceConfig>();
	}

	/// <summary>
	/// Configuration for a storage namespace
	/// </summary>
	public class NamespaceConfig
	{
		/// <summary>
		/// Identifier for this namespace
		/// </summary>
		public string Id { get; set; } = String.Empty;

		/// <summary>
		/// Buckets within this namespace
		/// </summary>
		public List<BucketConfig> Buckets { get; set; } = new List<BucketConfig>();

		/// <summary>
		/// Access control for this namespace
		/// </summary>
		public UpdateAclRequest? Acl { get; set; }
	}

	/// <summary>
	/// Configuration for a bucket
	/// </summary>
	public class BucketConfig
	{
		/// <summary>
		/// Identifier for the bucket
		/// </summary>
		public string Id { get; set; } = String.Empty;
	}

	/// <summary>
	/// Describes the monetary cost of agents matching a particular criteris
	/// </summary>
	public class AgentRateConfig
	{
		/// <summary>
		/// Condition string
		/// </summary>
		[CbField("c")]
		public Condition? Condition { get; set; }

		/// <summary>
		/// Rate for this agent
		/// </summary>
		[CbField("r")]
		public double Rate { get; set; }
	}
}

