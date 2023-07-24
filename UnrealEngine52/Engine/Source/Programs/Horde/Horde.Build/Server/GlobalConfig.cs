// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Security.Claims;
using System.Text.Json.Serialization;
using EpicGames.Core;
using EpicGames.Horde.Common;
using EpicGames.Horde.Compute;
using EpicGames.Perforce;
using EpicGames.Serialization;
using Horde.Build.Acls;
using Horde.Build.Agents.Pools;
using Horde.Build.Agents.Software;
using Horde.Build.Configuration;
using Horde.Build.Projects;
using Horde.Build.Storage;
using Horde.Build.Streams;
using Horde.Build.Tools;
using Horde.Build.Users;
using Horde.Build.Utilities;
using Microsoft.Extensions.Options;

namespace Horde.Build.Server
{
	using UserId = ObjectId<IUser>;
	using ProjectId = StringId<ProjectConfig>;
	using StreamId = StringId<IStream>;
	using AgentSoftwareChannelName = StringId<AgentSoftwareChannels>;
	using ToolId = StringId<ITool>;

	/// <summary>
	/// Directive to merge config data from another source
	/// </summary>
	[ConfigIncludeContext]
	public class ConfigInclude
	{
		/// <summary>
		/// Path to the config data to be included. May be relative to the including file's location.
		/// </summary>
		[Required, ConfigInclude, ConfigRelativePath]
		public string Path { get; set; } = null!;
	}

	/// <summary>
	/// Global configuration
	/// </summary>
	[JsonSchema("https://unrealengine.com/horde/global")]
	[JsonSchemaCatalog("Horde Globals", "Horde global configuration file", "globals.json")]
	[ConfigIncludeRoot]
	public class GlobalConfig
	{
		/// <summary>
		/// Global server settings object
		/// </summary>
		[JsonIgnore]
		public ServerSettings ServerSettings { get; private set; } = null!;

		/// <summary>
		/// Unique identifier for this config revision. Useful to detect changes.
		/// </summary>
		[JsonIgnore]
		public string Revision { get; set; } = String.Empty;

		/// <summary>
		/// Other paths to include
		/// </summary>
		public List<ConfigInclude> Include { get; set; } = new List<ConfigInclude>();

		/// <summary>
		/// List of projects
		/// </summary>
		public List<ProjectConfig> Projects { get; set; } = new List<ProjectConfig>();

		/// <summary>
		/// List of pools
		/// </summary>
		public List<PoolConfig> Pools { get; set; } = new List<PoolConfig>();

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
		public List<AgentSoftwareConfig> Software { get; set; } = new List<AgentSoftwareConfig>();

		/// <summary>
		/// List of costs of a particular agent type
		/// </summary>
		public List<AgentRateConfig> Rates { get; set; } = new List<AgentRateConfig>();

		/// <summary>
		/// List of compute profiles
		/// </summary>
		public List<ComputeClusterConfig> Compute { get; set; } = new List<ComputeClusterConfig>();

		/// <summary>
		/// Device configuration
		/// </summary>
		public DeviceConfig? Devices { get; set; }

		/// <summary>
		/// List of tools hosted by the server
		/// </summary>
		public List<ToolConfig> Tools { get; set; } = new List<ToolConfig>();

		/// <summary>
		/// Maximum number of conforms to run at once
		/// </summary>
		public int MaxConformCount { get; set; }

		/// <summary>
		/// Storage configuration
		/// </summary>
		public StorageConfig Storage { get; set; } = new StorageConfig();

		/// <summary>
		/// Access control list
		/// </summary>
		public AclConfig Acl { get; set; } = new AclConfig();

		/// <summary>
		/// Enumerates all the streams
		/// </summary>
		[JsonIgnore]
		public IReadOnlyList<StreamConfig> Streams { get; private set; } = null!;

		private readonly Dictionary<ProjectId, ProjectConfig> _projectLookup = new Dictionary<ProjectId, ProjectConfig>();
		private readonly Dictionary<StreamId, StreamConfig> _streamLookup = new Dictionary<StreamId, StreamConfig>();
		private readonly Dictionary<ToolId, ToolConfig> _toolLookup = new Dictionary<ToolId, ToolConfig>();
		private readonly Dictionary<ClusterId, ComputeClusterConfig> _computeClusterLookup = new Dictionary<ClusterId, ComputeClusterConfig>();

		/// <summary>
		/// Called after the config file has been read
		/// </summary>
		public void PostLoad(ServerSettings serverSettings)
		{
			ServerSettings = serverSettings;

			Streams = Projects.SelectMany(x => x.Streams).ToList();

			_projectLookup.Clear();
			_streamLookup.Clear();
			foreach (ProjectConfig project in Projects)
			{
				_projectLookup.Add(project.Id, project);
				project.PostLoad(project.Id, this);

				foreach (StreamConfig stream in project.Streams)
				{
					_streamLookup.Add(stream.Id, stream);
				}
			}

			_toolLookup.Clear();
			foreach (ToolConfig tool in Tools)
			{
				_toolLookup.Add(tool.Id, tool);
				tool.PostLoad(this);
			}

			_computeClusterLookup.Clear();
			foreach (ComputeClusterConfig computeCluster in Compute)
			{
				_computeClusterLookup.Add(computeCluster.Id, computeCluster);
				computeCluster.PostLoad(this);
			}

			Storage.PostLoad(this);
		}

		/// <summary>
		/// Attempts to get configuration for a project from this object
		/// </summary>
		/// <param name="projectId">The stream identifier</param>
		/// <param name="config">Configuration for the stream</param>
		/// <returns>True if the stream configuration was found</returns>
		public bool TryGetProject(ProjectId projectId, [NotNullWhen(true)] out ProjectConfig? config) => _projectLookup.TryGetValue(projectId, out config);

		/// <summary>
		/// Attempts to get configuration for a stream from this object
		/// </summary>
		/// <param name="streamId">The stream identifier</param>
		/// <param name="config">Configuration for the stream</param>
		/// <returns>True if the stream configuration was found</returns>
		public bool TryGetStream(StreamId streamId, [NotNullWhen(true)] out StreamConfig? config) => _streamLookup.TryGetValue(streamId, out config);

		/// <summary>
		/// Attempts to get configuration for a tool from this object
		/// </summary>
		/// <param name="toolId">The tool identifier</param>
		/// <param name="config">Configuration for the stream</param>
		/// <returns>True if the stream configuration was found</returns>
		public bool TryGetTool(ToolId toolId, [NotNullWhen(true)] out ToolConfig? config) => _toolLookup.TryGetValue(toolId, out config);

		/// <summary>
		/// Attempts to get compute cluster configuration from this object
		/// </summary>
		/// <param name="clusterId">Compute cluster id</param>
		/// <param name="config">Receives the cluster configuration on success</param>
		/// <returns>True on success</returns>
		public bool TryGetComputeCluster(ClusterId clusterId, [NotNullWhen(true)] out ComputeClusterConfig? config) => _computeClusterLookup.TryGetValue(clusterId, out config);

		/// <summary>
		/// Authorizes a user to perform a given action
		/// </summary>
		/// <param name="action">The action being performed</param>
		/// <param name="user">The principal to validate</param>
		public bool Authorize(AclAction action, ClaimsPrincipal user)
		{
			return Acl.Authorize(action, user) ?? ServerSettings.Authorize(action, user);
		}

		/// <summary>
		/// Determines whether the given user can masquerade as a given user
		/// </summary>
		/// <param name="user"></param>
		/// <param name="userId"></param>
		/// <returns></returns>
		public bool AuthorizeAsUser(ClaimsPrincipal user, UserId userId)
		{
			UserId? currentUserId = user.GetUserId();
			if (currentUserId != null && currentUserId.Value == userId)
			{
				return true;
			}
			else
			{
				return Authorize(AclAction.Impersonate, user);
			}
		}

		/// <summary>
		/// Finds a perforce cluster with the given name or that contains the provided server
		/// </summary>
		/// <param name="name">Name of the cluster</param>
		/// <param name="serverAndPort">Find cluster which contains server</param>
		/// <returns></returns>
		public PerforceCluster GetPerforceCluster(string? name, string? serverAndPort = null)
		{
			PerforceCluster? cluster = FindPerforceCluster(name, serverAndPort);
			if (cluster == null)
			{
				throw new Exception($"Unknown Perforce cluster '{name}'");
			}
			return cluster;
		}

		/// <summary>
		/// Finds a perforce cluster with the given name or that contains the provided server
		/// </summary>
		/// <param name="name">Name of the cluster</param>
		/// <param name="serverAndPort">Find cluster which contains server</param>
		/// <returns></returns>
		public PerforceCluster? FindPerforceCluster(string? name, string? serverAndPort = null)
		{
			List<PerforceCluster> clusters = PerforceClusters;

			if (serverAndPort != null)
			{
				return clusters.FirstOrDefault(x => x.Servers.FirstOrDefault(server => String.Equals(server.ServerAndPort, serverAndPort, StringComparison.OrdinalIgnoreCase)) != null);
			}

			if (clusters.Count == 0)
			{
				clusters = DefaultClusters;
			}

			if (name == null)
			{
				return clusters.FirstOrDefault();
			}
			else
			{
				return clusters.FirstOrDefault(x => String.Equals(x.Name, name, StringComparison.OrdinalIgnoreCase));
			}
		}

		static List<PerforceCluster> DefaultClusters { get; } = GetDefaultClusters();

		static List<PerforceCluster> GetDefaultClusters()
		{
			PerforceServer server = new PerforceServer();
			server.ServerAndPort = PerforceSettings.Default.ServerAndPort;

			PerforceCluster cluster = new PerforceCluster();
			cluster.Name = "Default";
			cluster.CanImpersonate = false;
			cluster.Servers.Add(server);

			return new List<PerforceCluster> { cluster };
		}
	}

	/// <summary>
	/// Profile for executing compute requests
	/// </summary>
	[DebuggerDisplay("{Id}")]
	public class ComputeClusterConfig
	{
		/// <summary>
		/// The owning global config instance
		/// </summary>
		[JsonIgnore]
		public GlobalConfig GlobalConfig { get; private set; } = null!;

		/// <summary>
		/// Name of the partition
		/// </summary>
		public ClusterId Id { get; set; } = new ClusterId("default");

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
		/// Filter for agents to include
		/// </summary>
		public Condition? Condition { get; set; }

		/// <summary>
		/// Access control list
		/// </summary>
		public AclConfig? Acl { get; set; }

		/// <summary>
		/// Callback post loading this config file
		/// </summary>
		/// <param name="globalConfig">The global config instance</param>
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
	/// Configuration for a device platform 
	/// </summary>
	[DebuggerDisplay("{Id}")]
	public class DevicePlatformConfig
	{
		/// <summary>
		/// The id for this platform 
		/// </summary>
		public string Id { get; set; } = String.Empty;

		/// <summary>
		/// List of platform names for this device, which may be requested by Gauntlet 
		/// </summary>
		public List<string> Names { get; set; } = new List<string>();

		/// <summary>
		/// Model name for the high perf spec, which may be requested by Gauntlet (Deprecated)
		/// </summary>
		public string? LegacyPerfSpecHighModel { get; set; }
	}

	/// <summary>
	/// Configuration for devices
	/// </summary>
	public class DeviceConfig
	{
		/// <summary>
		/// List of device platforms
		/// </summary>
		public List<DevicePlatformConfig> Platforms { get; set; } = new List<DevicePlatformConfig>();
	}

	/// <summary>
	/// Selects different agent software versions by evaluating a condition
	/// </summary>
	[DebuggerDisplay("{Channel}")]
	public class AgentSoftwareConfig
	{
		/// <summary>
		/// Channel name
		/// </summary>
		public AgentSoftwareChannelName Channel { get; set; }

		/// <summary>
		/// Condition for using this channel
		/// </summary>
		public Condition? Condition { get; set; }
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

	/// <summary>
	/// How frequently the maintence window repeats
	/// </summary>
	public enum ScheduledDowntimeFrequency
	{
		/// <summary>
		/// Once
		/// </summary>
		Once,

		/// <summary>
		/// Every day
		/// </summary>
		Daily,

		/// <summary>
		/// Every week
		/// </summary>
		Weekly,
	}

	/// <summary>
	/// Settings for the maintenance window
	/// </summary>
	public class ScheduledDowntime
	{
		/// <summary>
		/// Start time
		/// </summary>
		public DateTimeOffset StartTime { get; set; }

		/// <summary>
		/// Finish time
		/// </summary>
		public DateTimeOffset FinishTime { get; set; }

		/// <summary>
		/// Frequency that the window repeats
		/// </summary>
		public ScheduledDowntimeFrequency Frequency { get; set; } = ScheduledDowntimeFrequency.Once;

		/// <summary>
		/// Gets the next scheduled downtime
		/// </summary>
		/// <param name="now">The current time</param>
		/// <returns>Start and finish time</returns>
		public (DateTimeOffset StartTime, DateTimeOffset FinishTime) GetNext(DateTimeOffset now)
		{
			TimeSpan offset = TimeSpan.Zero;
			if (Frequency == ScheduledDowntimeFrequency.Daily)
			{
				double days = (now - StartTime).TotalDays;
				if (days >= 1.0)
				{
					days -= days % 1.0;
				}
				offset = TimeSpan.FromDays(days);
			}
			else if (Frequency == ScheduledDowntimeFrequency.Weekly)
			{
				double days = (now - StartTime).TotalDays;
				if (days >= 7.0)
				{
					days -= days % 7.0;
				}
				offset = TimeSpan.FromDays(days);
			}
			return (StartTime + offset, FinishTime + offset);
		}

		/// <summary>
		/// Determines if this schedule is active
		/// </summary>
		/// <param name="now">The current time</param>
		/// <returns>True if downtime is active</returns>
		public bool IsActive(DateTimeOffset now)
		{
			if (Frequency == ScheduledDowntimeFrequency.Once)
			{
				return now >= StartTime && now < FinishTime;
			}
			else if (Frequency == ScheduledDowntimeFrequency.Daily)
			{
				double days = (now - StartTime).TotalDays;
				if (days < 0.0)
				{
					return false;
				}
				else
				{
					return (days % 1.0) < (FinishTime - StartTime).TotalDays;
				}
			}
			else if (Frequency == ScheduledDowntimeFrequency.Weekly)
			{
				double days = (now - StartTime).TotalDays;
				if (days < 0.0)
				{
					return false;
				}
				else
				{
					return (days % 7.0) < (FinishTime - StartTime).TotalDays;
				}
			}
			else
			{
				return false;
			}
		}
	}

	/// <summary>
	/// Path to a platform and stream to use for syncing AutoSDK
	/// </summary>
	public class AutoSdkWorkspace
	{
		/// <summary>
		/// Name of this workspace
		/// </summary>
		public string? Name { get; set; }

		/// <summary>
		/// The agent properties to check (eg. "OSFamily=Windows")
		/// </summary>
		public List<string> Properties { get; set; } = new List<string>();

		/// <summary>
		/// Username for logging in to the server
		/// </summary>
		public string? UserName { get; set; }

		/// <summary>
		/// Stream to use
		/// </summary>
		[Required]
		public string? Stream { get; set; }
	}

	/// <summary>
	/// Information about an individual Perforce server
	/// </summary>
	public class PerforceServer
	{
		/// <summary>
		/// The server and port. The server may be a DNS entry with multiple records, in which case it will be actively load balanced.
		/// </summary>
		public string ServerAndPort { get; set; } = "perforce:1666";

		/// <summary>
		/// Whether to query the healthcheck address under each server
		/// </summary>
		public bool HealthCheck { get; set; }

		/// <summary>
		/// Whether to resolve the DNS entries and load balance between different hosts
		/// </summary>
		public bool ResolveDns { get; set; }

		/// <summary>
		/// Maximum number of simultaneous conforms on this server
		/// </summary>
		public int MaxConformCount { get; set; }

		/// <summary>
		/// Optional condition for a machine to be eligable to use this server
		/// </summary>
		public Condition? Condition { get; set; }

		/// <summary>
		/// List of properties for an agent to be eligable to use this server
		/// </summary>
		public List<string>? Properties { get; set; }
	}

	/// <summary>
	/// Credentials for a Perforce user
	/// </summary>
	public class PerforceCredentials
	{
		/// <summary>
		/// The username
		/// </summary>
		public string UserName { get; set; } = String.Empty;

		/// <summary>
		/// Password for the user
		/// </summary>
		public string Password { get; set; } = String.Empty;
	}

	/// <summary>
	/// Information about a cluster of Perforce servers. 
	/// </summary>
	[DebuggerDisplay("{Name}")]
	public class PerforceCluster
	{
		/// <summary>
		/// The default cluster name
		/// </summary>
		public const string DefaultName = "Default";

		/// <summary>
		/// Name of the cluster
		/// </summary>
		[Required]
		public string Name { get; set; } = null!;

		/// <summary>
		/// Username for Horde to log in to this server. Will use the default user if not set.
		/// </summary>
		public string? ServiceAccount { get; set; }

		/// <summary>
		/// Whether the service account can impersonate other users
		/// </summary>
		public bool CanImpersonate { get; set; } = true;

		/// <summary>
		/// List of servers
		/// </summary>
		public List<PerforceServer> Servers { get; set; } = new List<PerforceServer>();

		/// <summary>
		/// List of server credentials
		/// </summary>
		public List<PerforceCredentials> Credentials { get; set; } = new List<PerforceCredentials>();

		/// <summary>
		/// List of autosdk streams
		/// </summary>
		public List<AutoSdkWorkspace> AutoSdk { get; set; } = new List<AutoSdkWorkspace>();
	}
}

