// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel.DataAnnotations;
using System.Diagnostics;
using System.Security.Claims;
using System.Text.Json.Serialization;
using EpicGames.Horde.Acls;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Tools;
using Horde.Server.Acls;
using Horde.Server.Server;
using Horde.Server.Storage;

namespace Horde.Server.Tools
{
	/// <summary>
	/// Options for configuring a tool
	/// </summary>
	[DebuggerDisplay("{Id}")]
	public class ToolConfig
	{
		/// <summary>
		/// The global config containing this tool
		/// </summary>
		[JsonIgnore]
		public GlobalConfig GlobalConfig { get; private set; } = null!;

		/// <summary>
		/// Unique identifier for the tool
		/// </summary>
		[Required]
		public ToolId Id { get; set; }

		/// <summary>
		/// Name of the tool
		/// </summary>
		[Required]
		public string Name { get; set; }

		/// <summary>
		/// Description for the tool
		/// </summary>
		public string Description { get; set; }

		/// <summary>
		/// Category for the tool. Will cause the tool to be shown in a different tab in the dashboard.
		/// </summary>
		public string? Category { get; set; }

		/// <summary>
		/// Whether this tool should be exposed for download on a public endpoint without authentication
		/// </summary>
		public bool Public { get; set; }

		/// <summary>
		/// Whether to show this tool for download in the UGS tools menu
		/// </summary>
		public bool ShowInUgs { get; set; }

		/// <summary>
		/// Whether to show this tool for download in the dashboard
		/// </summary>
		public bool ShowInDashboard { get; set; } = true;

		/// <summary>
		/// Default namespace for new deployments of this tool
		/// </summary>
		public NamespaceId NamespaceId { get; set; } = Namespace.Tools;

		/// <summary>
		/// Permissions for the tool
		/// </summary>
		public AclConfig Acl { get; set; } = new AclConfig();

		/// <summary>
		/// Default constructor for serialization
		/// </summary>
		public ToolConfig()
		{
			Name = String.Empty;
			Description = String.Empty;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public ToolConfig(ToolId id)
		{
			Id = id;
			Name = id.ToString();
			Description = String.Empty;
		}

		/// <summary>
		/// Called after the config has been read
		/// </summary>
		/// <param name="globalConfig">Parent GlobalConfig object</param>
		public void PostLoad(GlobalConfig globalConfig)
		{
			GlobalConfig = globalConfig;
			Acl.PostLoad(globalConfig.Acl, $"tool:{Id}");
		}

		/// <inheritdoc cref="AclConfig.Authorize(AclAction, ClaimsPrincipal)"/>
		public bool Authorize(AclAction action, ClaimsPrincipal user)
			=> Acl.Authorize(action, user);
	}

	/// <summary>
	/// Options for a new deployment
	/// </summary>
	public class ToolDeploymentConfig
	{
		/// <inheritdoc cref="IToolDeployment.Version"/>
		public string Version { get; set; } = "Unknown";

		/// <inheritdoc cref="IToolDeployment.Duration"/>
		public TimeSpan Duration { get; set; }

		/// <summary>
		/// Whether to create the deployment in a paused state
		/// </summary>
		public bool CreatePaused { get; set; }
	}
}
