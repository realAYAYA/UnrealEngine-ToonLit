// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel.DataAnnotations;
using System.Diagnostics;
using System.Security.Claims;
using System.Text.Json.Serialization;
using Horde.Server.Acls;
using Horde.Server.Server;
using Horde.Server.Utilities;

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
		/// Whether this tool should be exposed for download on a public endpoint without authentication
		/// </summary>
		public bool Public { get; set; }

		/// <summary>
		/// Permissions for the tool
		/// </summary>
		public AclConfig? Acl { get; set; }

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
