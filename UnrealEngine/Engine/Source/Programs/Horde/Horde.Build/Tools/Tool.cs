// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Storage;
using Horde.Build.Acls;
using Horde.Build.Utilities;
using MongoDB.Bson.Serialization.Attributes;
using System;
using System.Collections.Generic;
using System.Security.Claims;
using System.Text.Json.Serialization;
using System.Threading.Tasks;

namespace Horde.Build.Tools
{
	using ToolId = StringId<Tool>;
	using ToolDeploymentId = ObjectId<ToolDeployment>;

	/// <summary>
	/// Options for configuring a tool
	/// </summary>
	public class ToolOptions
	{
		/// <inheritdoc cref="VersionedDocument{ToolId, Tool}.Id"/>
		public ToolId Id { get; set; }

		/// <inheritdoc cref="Tool.Name"/>
		public string Name { get; set; }

		/// <inheritdoc cref="Tool.Description"/>
		public string Description { get; set; }

		/// <inheritdoc cref="Tool.Public"/>
		public bool Public { get; set; }

		/// <inheritdoc cref="Tool.Acl"/>
		public AclV2? Acl { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		public ToolOptions(ToolId id)
		{
			Id = id;
			Name = id.ToString();
			Description = String.Empty;
		}
	}

	/// <summary>
	/// Describes a standalone, external tool hosted and deployed by Horde. Provides basic functionality for performing
	/// gradual roll-out, versioning, etc...
	/// </summary>
	public class Tool : VersionedDocument<ToolId, Tool>
	{
		/// <summary>
		/// Name of the tool
		/// </summary>
		[BsonElement("nam")]
		[JsonPropertyOrder(2)]
		public string Name { get; set; }

		/// <summary>
		/// Description for the tool
		/// </summary>
		[BsonElement("dsc")]
		[JsonPropertyOrder(3)]
		public string Description { get; set; }

		/// <summary>
		/// Current deployments of this tool, sorted by time.
		/// </summary>
		[BsonElement("dep")]
		[JsonPropertyOrder(4)]
		public List<ToolDeployment> Deployments { get; set; } = new List<ToolDeployment>();

		/// <summary>
		/// Whether this tool should be exposed for download on a public endpoint without authentication
		/// </summary>
		[BsonElement("pub")]
		[JsonPropertyOrder(5)]
		public bool Public { get; set; }

		/// <summary>
		/// Access list for this tool
		/// </summary>
		[BsonElement("acl")]
		[JsonPropertyOrder(3), JsonIgnore(Condition = JsonIgnoreCondition.WhenWritingNull)]
		public AclV2? Acl
		{
			get => _acl;
			set => _acl = (value is AclV2 && !value.IsDefault()) ? value : null;
		}

		private AclV2? _acl;

		/// <summary>
		/// Constructor
		/// </summary>
		[JsonConstructor]
		[BsonConstructor]
		public Tool(ToolId id)
			: base(id)
		{
			Id = id;
			Name = id.ToString();
			Description = String.Empty;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public Tool(ToolOptions config)
			: base(config.Id)
		{
			Name = config.Name;
			Description = config.Description;
			Public = config.Public;
			Acl = config.Acl;
		}

		/// <inheritdoc/>
		public override Tool UpgradeToLatest() => this;

		/// <summary>
		/// Updates the state values derived from the current time
		/// </summary>
		/// <param name="utcNow"></param>
		public void UpdateTemporalState(DateTime utcNow)
		{
			foreach (ToolDeployment deployment in Deployments)
			{
				deployment.UpdateTemporalState(utcNow);
			}
		}

		/// <summary>
		/// Test whether a user can perform an action on this tool 
		/// </summary>
		public Task<bool> AuthorizeAsync(AclAction action, ClaimsPrincipal user, AclService aclService, GlobalPermissionsCache? cache)
		{
			bool? result = Acl?.Authorize(action, user);
			if (result == null)
			{
				return aclService.AuthorizeAsync(action, user, cache);
			}
			else
			{
				return Task.FromResult(result.Value);
			}
		}
	}

	/// <summary>
	/// Current state of a tool's deployment
	/// </summary>
	public enum ToolDeploymentState
	{
		/// <summary>
		/// The deployment is ongoing
		/// </summary>
		Active,

		/// <summary>
		/// The deployment should be paused at its current state
		/// </summary>
		Paused,

		/// <summary>
		/// Deployment of this version is complete
		/// </summary>
		Complete,

		/// <summary>
		/// The deployment has been cancelled.
		/// </summary>
		Cancelled,
	}

	/// <summary>
	/// Options for a new deployment
	/// </summary>
	public class ToolDeploymentOptions
	{
		/// <summary>
		/// Default mime types for deployment data
		/// </summary>
		public const string DefaultMimeType = "application/zip";

		/// <inheritdoc cref="ToolDeployment.Version"/>
		public string Version { get; set; } = "Unknown";

		/// <inheritdoc cref="ToolDeployment.Duration"/>
		public TimeSpan Duration { get; set; }

		/// <inheritdoc cref="ToolDeployment.MimeType"/>
		public string MimeType { get; set; } = DefaultMimeType;

		/// <summary>
		/// Whether to create the deployment in a paused state
		/// </summary>
		public bool CreatePaused { get; set; }
	}

	/// <summary>
	/// Deployment of a tool
	/// </summary>
	public class ToolDeployment
	{
		/// <summary>
		/// Identifier for this deployment. A new identifier will be assigned to each created instance, so an identifier corresponds to a unique deployment.
		/// </summary>
		[JsonPropertyOrder(0)]
		public ToolDeploymentId Id { get; set; }

		/// <summary>
		/// Descriptive version string for this tool revision
		/// </summary>
		[BsonElement("ver")]
		[JsonPropertyOrder(1)]
		public string Version { get; set; }

		/// <summary>
		/// Current state of this deployment
		/// </summary>
		[BsonIgnore]
		[JsonPropertyOrder(2)]
		public ToolDeploymentState State { get; set; }

		/// <summary>
		/// Current progress of the deployment
		/// </summary>
		[BsonIgnore]
		[JsonPropertyOrder(3)]
		public double Progress { get; set; }

		/// <summary>
		/// Progress in the deployment at <see cref="StartedAt"/>, from 0-1.
		/// </summary>
		[JsonIgnore]
		[BsonElement("bpr")]
		public double BaseProgress { get; set; }

		/// <summary>
		/// Last time at which the progress started. Set to null if the deployment was paused.
		/// </summary>
		[JsonPropertyOrder(4)]
		[BsonElement("stm")]
		public DateTime? StartedAt { get; set; }

		/// <summary>
		/// Length of time over which to make the deployment
		/// </summary>
		[BsonElement("dur")]
		[JsonPropertyOrder(5)]
		public TimeSpan Duration { get; set; }

		/// <summary>
		/// Mime-type for the data
		/// </summary>
		[BsonElement("mtp")]
		[JsonPropertyOrder(6)]
		public string MimeType { get; set; }

		/// <summary>
		/// Reference to this tool in Horde Storage.
		/// </summary>
		[BsonElement("ref")]
		[JsonPropertyOrder(7)]
		public RefId RefId { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		public ToolDeployment(ToolDeploymentId id)
		{
			Id = id;
			Version = String.Empty;
			MimeType = ToolDeploymentOptions.DefaultMimeType;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public ToolDeployment(ToolDeploymentId id, ToolDeploymentOptions options, RefId refId)
		{
			Id = id;
			Version = options.Version;
			Duration = options.Duration;
			MimeType = options.MimeType;
			RefId = refId;
		}

		/// <summary>
		/// Update the computed deployment state
		/// </summary>
		/// <param name="utcNow"></param>
		public void UpdateTemporalState(DateTime utcNow)
		{
			if (BaseProgress >= 1.0)
			{
				State = ToolDeploymentState.Complete;
				Progress = 1.0;
			}
			else if (StartedAt == null)
			{
				State = ToolDeploymentState.Paused;
				Progress = BaseProgress;
			}
			else if (Duration > TimeSpan.Zero)
			{
				State = ToolDeploymentState.Active;
				Progress = Math.Clamp((utcNow - StartedAt.Value) / Duration, 0.0, 1.0);
			}
			else
			{
				State = ToolDeploymentState.Complete;
				Progress = 1.0;
			}
		}

		/// <summary>
		/// Get the updated progress value for a particular time
		/// </summary>
		/// <param name="utcNow">The current time</param>
		/// <returns></returns>
		public double GetProgressValue(DateTime utcNow)
		{
			if (StartedAt == null)
			{
				return Progress;
			}
			else if (Duration > TimeSpan.Zero)
			{
				return Math.Clamp((utcNow - StartedAt.Value) / Duration, 0.0, 1.0);
			}
			else
			{
				return 1.0;
			}
		}
	}
}
