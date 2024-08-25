// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Pools;
using EpicGames.Horde.Agents.Sessions;
using EpicGames.Horde.Common;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Streams;
using EpicGames.Horde.Tools;
using Google.Protobuf.WellKnownTypes;
using Horde.Server.Agents.Pools;
using Horde.Server.Perforce;
using Horde.Server.Server;
using Horde.Server.Streams;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Messages;
using HordeCommon.Rpc.Tasks;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;

namespace Horde.Server.Agents
{
	/// <summary>
	/// Information about a workspace synced to an agent
	/// </summary>
	public class AgentWorkspaceInfo
	{
		/// <summary>
		/// Name of the Perforce cluster to use
		/// </summary>
		public string? Cluster { get; set; }

		/// <summary>
		/// User to log into Perforce with (eg. buildmachine)
		/// </summary>
		public string? UserName { get; set; }

		/// <summary>
		/// Identifier to distinguish this workspace from other workspaces
		/// </summary>
		public string Identifier { get; set; }

		/// <summary>
		/// The stream to sync
		/// </summary>
		public string Stream { get; set; }

		/// <summary>
		/// Custom view for the workspace
		/// </summary>
		public List<string>? View { get; set; }

		/// <summary>
		/// Whether to use an incremental workspace
		/// </summary>
		public bool Incremental { get; set; }

		/// <summary>
		/// Method to use when syncing/materializing data from Perforce
		/// </summary>
		public string? Method { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="cluster">Name of the Perforce cluster</param>
		/// <param name="userName">User to log into Perforce with (eg. buildmachine)</param>
		/// <param name="identifier">Identifier to distinguish this workspace from other workspaces</param>
		/// <param name="stream">The stream to sync</param>
		/// <param name="view">Custom view for the workspace</param>
		/// <param name="incremental">Whether to use an incremental workspace</param>
		/// <param name="method">Method to use when syncing/materializing data from Perforce</param>
		public AgentWorkspaceInfo(string? cluster, string? userName, string identifier, string stream, List<string>? view, bool incremental, string? method)
		{
			if (!String.IsNullOrEmpty(cluster))
			{
				Cluster = cluster;
			}
			if (!String.IsNullOrEmpty(userName))
			{
				UserName = userName;
			}
			Identifier = identifier;
			Stream = stream;
			View = view;
			Incremental = incremental;
			Method = method;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="workspace">RPC message to construct from</param>
		public AgentWorkspaceInfo(AgentWorkspace workspace)
			: this(workspace.ConfiguredCluster, workspace.ConfiguredUserName, workspace.Identifier, workspace.Stream, (workspace.View.Count > 0) ? workspace.View.ToList() : null, workspace.Incremental, workspace.Method)
		{
		}

		/// <summary>
		/// Gets a digest of the settings for this workspace
		/// </summary>
		/// <returns>Digest for the workspace settings</returns>
		public string GetDigest()
		{
#pragma warning disable CA5351 // Do Not Use Broken Cryptographic Algorithms
			using (MD5 hasher = MD5.Create())
			{
				byte[] data = BsonExtensionMethods.ToBson(this);
				return BitConverter.ToString(hasher.ComputeHash(data)).Replace("-", "", StringComparison.Ordinal);
			}
#pragma warning restore CA5351
		}

		/// <inheritdoc/>
		public override bool Equals(object? obj)
		{
			AgentWorkspaceInfo? other = obj as AgentWorkspaceInfo;
			if (other == null)
			{
				return false;
			}
			if (Cluster != other.Cluster || UserName != other.UserName || Identifier != other.Identifier || Stream != other.Stream || Incremental != other.Incremental)
			{
				return false;
			}
			if (!Enumerable.SequenceEqual(View ?? new List<string>(), other.View ?? new List<string>()))
			{
				return false;
			}
			return true;
		}

		/// <inheritdoc/>
		public override int GetHashCode()
		{
			return HashCode.Combine(Cluster, UserName, Identifier, Stream, Incremental); // Ignore 'View' for now
		}

		/// <summary>
		/// Checks if two workspace sets are equivalent, ignoring order
		/// </summary>
		/// <param name="workspacesA">First list of workspaces</param>
		/// <param name="workspacesB">Second list of workspaces</param>
		/// <returns>True if the sets are equivalent</returns>
		public static bool SetEquals(IReadOnlyList<AgentWorkspaceInfo> workspacesA, IReadOnlyList<AgentWorkspaceInfo> workspacesB)
		{
			HashSet<AgentWorkspaceInfo> workspacesSetA = new HashSet<AgentWorkspaceInfo>(workspacesA);
			return workspacesSetA.SetEquals(workspacesB);
		}

		/// <summary>
		/// Converts this workspace to an RPC message
		/// </summary>
		/// <param name="server">The Perforce server</param>
		/// <param name="credentials">Credentials for the server</param>
		/// <returns>The RPC message</returns>
		public AgentWorkspace ToRpcMessage(IPerforceServer server, PerforceCredentials? credentials)
		{
			// Construct the message
			AgentWorkspace result = new AgentWorkspace
			{
				ConfiguredCluster = Cluster,
				ConfiguredUserName = UserName,
				ServerAndPort = server.ServerAndPort,
				UserName = credentials?.UserName ?? UserName,
				Password = credentials?.Password,
				Ticket = credentials?.Ticket,
				Identifier = Identifier,
				Stream = Stream,
				Incremental = Incremental,
				Partitioned = server.SupportsPartitionedWorkspaces,
				Method = Method ?? String.Empty
			};

			if (View != null)
			{
				result.View.AddRange(View);
			}

			return result;
		}
	}

	/// <summary>
	/// Configuration for an AutoSDK workspace
	/// </summary>
	public class AutoSdkConfig
	{
		/// <summary>
		/// Disables use of AutoSDK.
		/// </summary>
		public static AutoSdkConfig None { get; } = new AutoSdkConfig(Array.Empty<string>());

		/// <summary>
		/// Syncs the entire AutoSDK folder.
		/// </summary>
		public static AutoSdkConfig Full { get; } = new AutoSdkConfig(null);

		/// <summary>
		/// Additive filter for paths to include in the workspace
		/// </summary>
		public List<string> View { get; set; } = new List<string>();

		/// <summary>
		/// Constructor
		/// </summary>
		public AutoSdkConfig()
		{
			View = new List<string>();
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="filter">Filter for the workspace</param>
		public AutoSdkConfig(IEnumerable<string>? filter)
		{
			if (filter == null)
			{
				View = new List<string> { "..." };
			}
			else
			{
				View = filter.OrderBy(x => x).Distinct().ToList();
			}
		}

		/// <summary>
		/// Merge two workspaces together
		/// </summary>
		[return: NotNullIfNotNull("lhs")]
		[return: NotNullIfNotNull("rhs")]
		public static AutoSdkConfig? Merge(AutoSdkConfig? lhs, AutoSdkConfig? rhs)
		{
			if (lhs?.View == null || lhs.View.Count == 0)
			{
				return rhs;
			}
			if (rhs?.View == null || rhs.View.Count == 0)
			{
				return lhs;
			}

			return new AutoSdkConfig(Enumerable.Concat(lhs.View, rhs.View));
		}

		/// <summary>
		/// Test whether two views are equal
		/// </summary>
		public static bool Equals(AutoSdkConfig? lhs, AutoSdkConfig? rhs)
		{
			if (lhs?.View == null || lhs.View.Count == 0)
			{
				return rhs?.View == null || rhs.View.Count == 0;
			}
			if (rhs?.View == null || rhs.View.Count == 0)
			{
				return false;
			}

			return Enumerable.SequenceEqual(lhs.View, rhs.View);
		}
	}

	/// <summary>
	/// Document describing an active lease
	/// </summary>
	public class AgentLease
	{
		/// <summary>
		/// Name of this lease
		/// </summary>
		[BsonRequired]
		public LeaseId Id { get; set; }

		/// <summary>
		/// The parent lease id
		/// </summary>
		public LeaseId? ParentId { get; set; }

		/// <summary>
		/// Name of this lease
		/// </summary>
		public string Name { get; set; }

		/// <summary>
		/// The current state of the lease
		/// </summary>
		public LeaseState State { get; set; }

		/// <summary>
		/// The stream for this lease
		/// </summary>
		public StreamId? StreamId { get; set; }

		/// <summary>
		/// The pool for this lease
		/// </summary>
		public PoolId? PoolId { get; set; }

		/// <summary>
		/// Optional log for this lease
		/// </summary>
		public LogId? LogId { get; set; }

		/// <summary>
		/// Time at which the lease started
		/// </summary>
		[BsonRequired]
		public DateTime StartTime { get; set; }

		/// <summary>
		/// Time at which the lease should be terminated
		/// </summary>
		public DateTime? ExpiryTime { get; set; }

		/// <summary>
		/// Flag indicating whether this lease has been accepted by the agent
		/// </summary>
		public bool Active { get; set; }

		/// <summary>
		/// Resources used by this lease
		/// </summary>
		public IReadOnlyDictionary<string, int>? Resources { get; set; }

		/// <summary>
		/// Whether the lease requires exclusive access to the agent
		/// </summary>
		public bool Exclusive { get; set; }

		/// <summary>
		/// For leases in the pending state, encodes an "any" protobuf containing the payload for the agent to execute the lease.
		/// </summary>
		public byte[]? Payload { get; set; }

		/// <summary>
		/// Private constructor
		/// </summary>
		[BsonConstructor]
		private AgentLease()
		{
			Name = String.Empty;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="id">Identifier for the lease</param>
		/// <param name="parentId">The parent lease id</param>
		/// <param name="name">Name of this lease</param>
		/// <param name="streamId"></param>
		/// <param name="poolId"></param>
		/// <param name="logId">Unique id for the log</param>
		/// <param name="state">State for the lease</param>
		/// <param name="resources">Resources required for this lease</param>
		/// <param name="exclusive">Whether to reserve the entire device</param>
		/// <param name="payload">Encoded "any" protobuf describing the contents of the payload</param>
		public AgentLease(LeaseId id, LeaseId? parentId, string name, StreamId? streamId, PoolId? poolId, LogId? logId, LeaseState state, IReadOnlyDictionary<string, int>? resources, bool exclusive, byte[]? payload)
		{
			Id = id;
			ParentId = parentId;
			Name = name;
			StreamId = streamId;
			PoolId = poolId;
			LogId = logId;
			State = state;
			Resources = resources;
			Exclusive = exclusive;
			Payload = payload;
			StartTime = DateTime.UtcNow;
		}

		/// <summary>
		/// Determines if this is a conform lease
		/// </summary>
		/// <returns>True if this is a conform lease</returns>
		public bool IsConformLease()
		{
			if (Payload != null)
			{
				Any basePayload = Any.Parser.ParseFrom(Payload);
				if (basePayload.Is(ConformTask.Descriptor))
				{
					return true;
				}
			}
			return false;
		}

		/// <summary>
		/// Converts this lease to an RPC message
		/// </summary>
		/// <returns>RPC message</returns>
		public HordeCommon.Rpc.Messages.Lease ToRpcMessage()
		{
			HordeCommon.Rpc.Messages.Lease lease = new HordeCommon.Rpc.Messages.Lease();
			lease.Id = Id.ToString();
			lease.Payload = Google.Protobuf.WellKnownTypes.Any.Parser.ParseFrom(Payload);
			lease.State = (RpcLeaseState)State;
			return lease;
		}
	}

	/// <summary>
	/// Well-known property names for agents
	/// </summary>
	static class KnownPropertyNames
	{
		/// <summary>
		/// The agent id
		/// </summary>
		public const string Id = "Id";

		/// <summary>
		/// The operating system (Linux, MacOS, Windows)
		/// </summary>
		public const string OsFamily = "OSFamily";

		/// <summary>
		/// Whether the agent is a .NET self-contained app
		/// </summary>
		public const string SelfContained = "SelfContained";

		/// <summary>
		/// Pools that this agent belongs to
		/// </summary>
		public const string Pool = "Pool";

		/// <summary>
		/// Pools requested by the agent to join when registering with server
		/// </summary>
		public const string RequestedPools = "RequestedPools";

		/// <summary>
		/// Number of logical cores
		/// </summary>
		public const string LogicalCores = "LogicalCores";

		/// <summary>
		/// Amount of RAM, in GB
		/// </summary>
		public const string Ram = "RAM";

		/// <summary>
		/// AWS: Instance ID
		/// </summary>
		public const string AwsInstanceId = "aws-instance-id";

		/// <summary>
		/// AWS: Instance type
		/// </summary>
		public const string AwsInstanceType = "aws-instance-type";
	}

	/// <summary>
	/// Mirrors an Agent document in the database
	/// </summary>
	public interface IAgent
	{
		/// <summary>
		/// Identifier for this agent.
		/// </summary>
		public AgentId Id { get; }

		/// <summary>
		/// The current session id, if it's online
		/// </summary>
		public SessionId? SessionId { get; }

		/// <summary>
		/// Time at which the current session expires. 
		/// </summary>
		public DateTime? SessionExpiresAt { get; }

		/// <summary>
		/// Current status of this agent
		/// </summary>
		public AgentStatus Status { get; }

		/// <summary>
		/// Time at which last status change took place.
		/// </summary>
		public DateTime? LastStatusChange { get; }

		/// <summary>
		/// Whether the agent is enabled
		/// </summary>
		public bool Enabled { get; }

		/// <summary>
		/// Whether the agent is ephemeral
		/// </summary>
		public bool Ephemeral { get; }

		/// <summary>
		/// Whether the agent should be included on the dashboard. This is set to true for ephemeral agents once they are no longer online, or agents that are explicitly deleted.
		/// </summary>
		public bool Deleted { get; }

		/// <summary>
		/// Version of the software running on this agent
		/// </summary>
		public string? Version { get; }

		/// <summary>
		/// Arbitrary comment for the agent (useful for disable reasons etc)
		/// </summary>
		public string? Comment { get; }

		/// <summary>
		/// List of properties for this agent
		/// </summary>
		public IReadOnlyList<string> Properties { get; }

		/// <summary>
		/// List of resources available to the agent
		/// </summary>
		public IReadOnlyDictionary<string, int> Resources { get; }

		/// <summary>
		/// Last upgrade that was attempted
		/// </summary>
		public string? LastUpgradeVersion { get; }

		/// <summary>
		/// Time that which the last upgrade was attempted
		/// </summary>
		public DateTime? LastUpgradeTime { get; }

		/// <summary>
		/// Number of times an upgrade job has failed
		/// </summary>
		public int? UpgradeAttemptCount { get; }

		/// <summary>
		/// Dynamically applied pools
		/// </summary>
		public IReadOnlyList<PoolId> DynamicPools { get; }

		/// <summary>
		/// List of manually assigned pools for agent
		/// </summary>
		public IReadOnlyList<PoolId> ExplicitPools { get; }

		/// <summary>
		/// Whether a conform is requested
		/// </summary>
		public bool RequestConform { get; }

		/// <summary>
		/// Whether a full conform is requested
		/// </summary>
		public bool RequestFullConform { get; }

		/// <summary>
		/// Whether a machine restart is requested
		/// </summary>
		public bool RequestRestart { get; }

		/// <summary>
		/// Whether the machine should be shutdown
		/// </summary>
		public bool RequestShutdown { get; }

		/// <summary>
		/// Whether a forced machine restart is requested
		/// </summary>
		public bool RequestForceRestart { get; }

		/// <summary>
		/// The reason for the last agent shutdown
		/// </summary>
		public string? LastShutdownReason { get; }

		/// <summary>
		/// List of workspaces currently synced to this machine
		/// </summary>
		public IReadOnlyList<AgentWorkspaceInfo> Workspaces { get; }

		/// <summary>
		/// Time at which the last conform job ran
		/// </summary>
		public DateTime LastConformTime { get; }

		/// <summary>
		/// Number of times a conform job has failed
		/// </summary>
		public int? ConformAttemptCount { get; }

		/// <summary>
		/// Array of active leases.
		/// </summary>
		public IReadOnlyList<AgentLease> Leases { get; }

		/// <summary>
		/// Key used to validate that a particular enrollment is still valid for this agent
		/// </summary>
		public string EnrollmentKey { get; }

		/// <summary>
		/// Last time that the agent was modified
		/// </summary>
		public DateTime UpdateTime { get; }

		/// <summary>
		/// Update counter for this document. Any updates should compare-and-swap based on the value of this counter, or increment it in the case of server-side updates.
		/// </summary>
		public uint UpdateIndex { get; }
	}

	/// <summary>
	/// Extension methods for IAgent
	/// </summary>
	public static class AgentExtensions
	{
		/// <summary>
		/// Default tool ID for agent software (multi-platform, shipped without a .NET runtime)
		/// This is being deprecated in favor of the platform-specific and self-contained versions of the agent below
		/// </summary>
		public static ToolId AgentToolId { get; } = new("horde-agent");

		/// <summary>
		/// Tool ID for Windows-specific and self-contained agent software
		/// </summary>
		public static ToolId AgentWinX64ToolId { get; } = new("horde-agent-win-x64");

		/// <summary>
		/// Tool ID for Linux-specific and self-contained agent software
		/// </summary>
		public static ToolId AgentLinuxX64ToolId { get; } = new("horde-agent-linux-x64");

		/// <summary>
		/// Tool ID for Mac-specific and self-contained agent software
		/// </summary>
		public static ToolId AgentMacX64ToolId { get; } = new("horde-agent-osx-x64");

		/// <summary>
		/// Gets the tool ID for the software the given agent should be running
		/// </summary>
		/// <param name="agent">Agent to check</param>
		/// <param name="globalConfig">Current global config</param>
		/// <returns>Identifier for the tool that the agent should be using</returns>
		public static ToolId GetSoftwareToolId(this IAgent agent, GlobalConfig globalConfig)
		{
			ToolId toolId = AgentToolId;

			if (agent.IsSelfContained())
			{
				// Skip support for condition-based software configs below by returning early when self-contained
				// Getting this wrong can lead to a self-contained agent getting non-self-contained updates and vice versa.
				return agent.GetOsFamily() switch
				{
					RuntimePlatform.Type.Windows => AgentWinX64ToolId,
					RuntimePlatform.Type.Linux => AgentLinuxX64ToolId,
					RuntimePlatform.Type.Mac => AgentMacX64ToolId,
					_ => throw new ArgumentOutOfRangeException("Unknown platform " + agent.GetOsFamily())
				};
			}

			foreach (AgentSoftwareConfig softwareConfig in globalConfig.Software)
			{
				if (softwareConfig.Condition != null && agent.SatisfiesCondition(softwareConfig.Condition))
				{
					toolId = softwareConfig.ToolId;
					break;
				}
			}
			return toolId;
		}

		/// <summary>
		/// Determines whether this agent is online
		/// </summary>
		/// <returns></returns>
		public static bool IsSessionValid(this IAgent agent, DateTime utcNow)
		{
			return agent.SessionId.HasValue && agent.SessionExpiresAt.HasValue && utcNow < agent.SessionExpiresAt.Value;
		}

		/// <summary>
		/// Tests whether an agent is in the given pool
		/// </summary>
		/// <param name="agent"></param>
		/// <param name="poolId"></param>
		/// <returns></returns>
		public static bool IsInPool(this IAgent agent, PoolId poolId)
		{
			return agent.DynamicPools.Contains(poolId) || agent.ExplicitPools.Contains(poolId);
		}

		/// <summary>
		/// Get all the pools for each agent
		/// </summary>
		/// <param name="agent">The agent to query</param>
		/// <returns></returns>
		public static IEnumerable<PoolId> GetPools(this IAgent agent)
		{
			foreach (PoolId poolId in agent.DynamicPools)
			{
				yield return poolId;
			}
			foreach (PoolId poolId in agent.ExplicitPools)
			{
				yield return poolId;
			}
		}

		/// <summary>
		/// Tests whether an agent has reported as being a self-contained .NET package
		/// </summary>
		/// <param name="agent">Agent to query</param>
		/// <returns>True if self-contained</returns>
		public static bool IsSelfContained(this IAgent agent)
		{
			List<string> values = agent.GetPropertyValues(KnownPropertyNames.SelfContained).ToList();
			return values.Count > 0 && values[0].Equals("true", StringComparison.OrdinalIgnoreCase);
		}

		/// <summary>
		/// Get operating system family of agent
		/// </summary>
		/// <param name="agent">Agent to query</param>
		/// <returns>Type of OS</returns>
		public static RuntimePlatform.Type? GetOsFamily(this IAgent agent)
		{
			List<string> values = agent.GetPropertyValues(KnownPropertyNames.OsFamily).ToList();
			if (values.Count == 0)
			{
				return null;
			}

			return values[0].ToUpperInvariant() switch
			{
				"WINDOWS" => RuntimePlatform.Type.Windows,
				"LINUX" => RuntimePlatform.Type.Linux,
				"MACOS" => RuntimePlatform.Type.Mac,
				_ => null
			};
		}

		/// <summary>
		/// Tests whether an agent has a particular property
		/// </summary>
		/// <param name="agent"></param>
		/// <param name="property"></param>
		/// <returns></returns>
		public static bool HasProperty(this IAgent agent, string property)
		{
			return agent.Properties.BinarySearch(property, StringComparer.OrdinalIgnoreCase) >= 0;
		}

		/// <summary>
		/// Finds property values from a sorted list of Name=Value pairs
		/// </summary>
		/// <param name="agent">The agent to query</param>
		/// <param name="name">Name of the property to find</param>
		/// <returns>Property values</returns>
		public static IEnumerable<string> GetPropertyValues(this IAgent agent, string name)
		{
			if (name.Equals(KnownPropertyNames.Id, StringComparison.OrdinalIgnoreCase))
			{
				yield return agent.Id.ToString();
			}
			else if (name.Equals(KnownPropertyNames.Pool, StringComparison.OrdinalIgnoreCase))
			{
				foreach (PoolId poolId in agent.GetPools())
				{
					yield return poolId.ToString();
				}
			}
			else
			{
				int index = agent.Properties.BinarySearch(name, StringComparer.OrdinalIgnoreCase);
				if (index < 0)
				{
					index = ~index;
					for (; index < agent.Properties.Count; index++)
					{
						string property = agent.Properties[index];
						if (property.Length <= name.Length || !property.StartsWith(name, StringComparison.OrdinalIgnoreCase) || property[name.Length] != '=')
						{
							break;
						}
						yield return property.Substring(name.Length + 1);
					}
				}
			}
		}

		/// <summary>
		/// Evaluates a condition against an agent
		/// </summary>
		/// <param name="agent">The agent to evaluate</param>
		/// <param name="condition">The condition to evaluate</param>
		/// <returns>True if the agent satisfies the condition</returns>
		public static bool SatisfiesCondition(this IAgent agent, Condition condition)
		{
			return condition.Evaluate(x => agent.GetPropertyValues(x));
		}

		/// <summary>
		/// Determine whether it's possible to add a lease for the given resources
		/// </summary>
		/// <param name="agent">The agent to create a lease for</param>
		/// <param name="requirements">Requirements for the lease</param>
		/// <param name="assignedResources">Receives the allocated resources</param>
		/// <returns>True if the new lease can be granted</returns>
		public static bool MeetsRequirements(this IAgent agent, Requirements requirements, Dictionary<string, int> assignedResources)
		{
			PoolId? poolId = null;
			if (!String.IsNullOrEmpty(requirements.Pool))
			{
				poolId = new PoolId(requirements.Pool);
			}
			return MeetsRequirements(agent, poolId, requirements.Condition, requirements.Resources, requirements.Exclusive, assignedResources);
		}

		/// <summary>
		/// Determine whether it's possible to add a lease for the given resources
		/// </summary>
		/// <param name="agent">The agent to create a lease for</param>
		/// <param name="poolId">Pool to take the machine from</param>
		/// <param name="condition">Condition to satisfy</param>
		/// <param name="resources">Resources required to execute</param>
		/// <param name="exclusive">Whether the lease needs to be executed exclusively on the machine</param>
		/// <param name="assignedResources">Resources allocated to the task</param>
		/// <returns>True if the new lease can be granted</returns>
		public static bool MeetsRequirements(this IAgent agent, PoolId? poolId, Condition? condition, Dictionary<string, ResourceRequirements>? resources, bool exclusive, Dictionary<string, int> assignedResources)
		{
			if (!agent.Enabled || agent.Status != AgentStatus.Ok)
			{
				return false;
			}
			if (agent.Leases.Any(x => x.Exclusive))
			{
				return false;
			}
			if (exclusive && agent.Leases.Any())
			{
				return false;
			}
			if (poolId.HasValue && !agent.IsInPool(poolId.Value))
			{
				return false;
			}
			if (condition != null && !agent.SatisfiesCondition(condition))
			{
				return false;
			}
			if (resources != null)
			{
				foreach ((string name, ResourceRequirements resourceRequirements) in resources)
				{
					int remainingCount;
					if (!agent.Resources.TryGetValue(name, out remainingCount))
					{
						return false;
					}
					foreach (AgentLease lease in agent.Leases)
					{
						if (lease.Resources != null)
						{
							int leaseCount;
							lease.Resources.TryGetValue(name, out leaseCount);
							remainingCount -= leaseCount;
						}
					}
					if (remainingCount < resourceRequirements.Min)
					{
						return false;
					}

					int allocatedCount;
					if (resourceRequirements.Max != null)
					{
						allocatedCount = Math.Min(resourceRequirements.Max.Value, remainingCount);
					}
					else
					{
						allocatedCount = resourceRequirements.Min;
					}
					assignedResources.Add(name, allocatedCount);
				}
			}
			return true;
		}

		/// <summary>
		/// Get the AutoSDK workspace required for an agent
		/// </summary>
		/// <param name="agent"></param>
		/// <param name="cluster">The perforce cluster to get a workspace for</param>
		/// <param name="pools">Pools that the agent belongs to</param>
		/// <returns></returns>
		public static AgentWorkspaceInfo? GetAutoSdkWorkspace(this IAgent agent, PerforceCluster cluster, IEnumerable<IPool> pools)
		{
			AutoSdkConfig? autoSdkConfig = null;
			foreach (IPool pool in pools)
			{
				autoSdkConfig = AutoSdkConfig.Merge(autoSdkConfig, pool.AutoSdkConfig);
			}
			if (autoSdkConfig == null)
			{
				return null;
			}

			return GetAutoSdkWorkspace(agent, cluster, autoSdkConfig);
		}

		/// <summary>
		/// Get the AutoSDK workspace required for an agent
		/// </summary>
		/// <param name="agent"></param>
		/// <param name="cluster">The perforce cluster to get a workspace for</param>
		/// <param name="autoSdkConfig">Configuration for autosdk</param>
		/// <returns></returns>
		public static AgentWorkspaceInfo? GetAutoSdkWorkspace(this IAgent agent, PerforceCluster cluster, AutoSdkConfig autoSdkConfig)
		{
			foreach (AutoSdkWorkspace autoSdk in cluster.AutoSdk)
			{
				if (autoSdk.Stream != null && autoSdk.Properties.All(x => agent.Properties.Contains(x)))
				{
					return new AgentWorkspaceInfo(cluster.Name, autoSdk.UserName, autoSdk.Name ?? "AutoSDK", autoSdk.Stream!, autoSdkConfig.View.ToList(), true, null);
				}
			}
			return null;
		}

		/// <summary>
		/// Converts this workspace to an RPC message
		/// </summary>
		/// <param name="agent">The agent to get a workspace for</param>
		/// <param name="workspace">The workspace definition</param>
		/// <param name="cluster">The global state</param>
		/// <param name="loadBalancer">The Perforce load balancer</param>
		/// <param name="workspaceMessages">List of messages</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The RPC message</returns>
		public static async Task<bool> TryAddWorkspaceMessageAsync(this IAgent agent, AgentWorkspaceInfo workspace, PerforceCluster cluster, PerforceLoadBalancer loadBalancer, IList<AgentWorkspace> workspaceMessages, CancellationToken cancellationToken)
		{
			// Find a matching server, trying to use a previously selected one if possible
			string? baseServerAndPort;
			string? serverAndPort;
			bool partitioned;

			AgentWorkspace? existingWorkspace = workspaceMessages.FirstOrDefault(x => x.ConfiguredCluster == workspace.Cluster);
			if (existingWorkspace != null)
			{
				baseServerAndPort = existingWorkspace.BaseServerAndPort;
				serverAndPort = existingWorkspace.ServerAndPort;
				partitioned = existingWorkspace.Partitioned;
			}
			else
			{
				if (cluster == null)
				{
					return false;
				}

				IPerforceServer? server = await loadBalancer.SelectServerAsync(cluster, agent, cancellationToken);
				if (server == null)
				{
					return false;
				}

				baseServerAndPort = server.BaseServerAndPort;
				serverAndPort = server.ServerAndPort;
				partitioned = server.SupportsPartitionedWorkspaces;
			}

			// Find the matching credentials for the desired user
			PerforceCredentials? credentials = null;
			if (cluster != null)
			{
				if (workspace.UserName == null)
				{
					credentials = cluster.Credentials.FirstOrDefault();
				}
				else
				{
					credentials = cluster.Credentials.FirstOrDefault(x => String.Equals(x.UserName, workspace.UserName, StringComparison.OrdinalIgnoreCase));
				}
			}

			// Construct the message
			AgentWorkspace result = new AgentWorkspace
			{
				ConfiguredCluster = workspace.Cluster,
				ConfiguredUserName = workspace.UserName,
				Cluster = cluster?.Name,
				BaseServerAndPort = baseServerAndPort,
				ServerAndPort = serverAndPort,
				UserName = credentials?.UserName ?? workspace.UserName,
				Password = credentials?.Password,
				Ticket = credentials?.Ticket,
				Identifier = workspace.Identifier,
				Stream = workspace.Stream,
				Incremental = workspace.Incremental,
				Partitioned = partitioned,
				Method = workspace.Method ?? String.Empty
			};

			if (workspace.View != null)
			{
				result.View.AddRange(workspace.View);
			}

			workspaceMessages.Add(result);
			return true;
		}

		/// <summary>
		/// Tries to get an agent workspace definition from the given type name
		/// </summary>
		/// <param name="streamConfig">The stream object</param>
		/// <param name="agentType">The agent type</param>
		/// <param name="workspace">Receives the agent workspace definition</param>
		/// <param name="autoSdkConfig">Receives the autosdk workspace config</param>
		/// <returns>True if the agent type was valid, and an agent workspace could be created</returns>
		public static bool TryGetAgentWorkspace(this StreamConfig streamConfig, AgentConfig agentType, [NotNullWhen(true)] out AgentWorkspaceInfo? workspace, out AutoSdkConfig? autoSdkConfig)
		{
			// Get the workspace settings
			if (agentType.Workspace == null)
			{
				// Use the default settings (fast switching workspace, clean)
				workspace = new AgentWorkspaceInfo(streamConfig.ClusterName, null, streamConfig.GetDefaultWorkspaceIdentifier(), streamConfig.Name, null, false, null);
				autoSdkConfig = AutoSdkConfig.Full;
				return true;
			}
			else
			{
				// Try to get the matching workspace type
				WorkspaceConfig? workspaceConfig;
				if (!streamConfig.WorkspaceTypes.TryGetValue(agentType.Workspace, out workspaceConfig))
				{
					workspace = null;
					autoSdkConfig = null;
					return false;
				}

				// Get the workspace identifier
				string identifier;
				if (workspaceConfig.Identifier != null)
				{
					identifier = workspaceConfig.Identifier;
				}
				else if (workspaceConfig.Incremental ?? false)
				{
					identifier = $"{streamConfig.GetEscapedName()}+{agentType.Workspace}";
				}
				else
				{
					identifier = streamConfig.GetDefaultWorkspaceIdentifier();
				}

				// Create the new workspace
				string cluster = workspaceConfig.Cluster ?? streamConfig.ClusterName;
				workspace = new AgentWorkspaceInfo(cluster, workspaceConfig.UserName, identifier, workspaceConfig.Stream ?? streamConfig.Name, workspaceConfig.View, workspaceConfig.Incremental ?? false, workspaceConfig.Method);
				autoSdkConfig = GetAutoSdkConfig(workspaceConfig, streamConfig);

				return true;
			}

			static AutoSdkConfig? GetAutoSdkConfig(WorkspaceConfig workspaceConfig, StreamConfig streamConfig)
			{
				AutoSdkConfig? autoSdkConfig = null;
				if (workspaceConfig.UseAutoSdk ?? true)
				{
					List<string> view = new List<string>();
					if (streamConfig.AutoSdkView != null)
					{
						view.AddRange(streamConfig.AutoSdkView);
					}
					if (workspaceConfig.AutoSdkView != null)
					{
						view.AddRange(workspaceConfig.AutoSdkView);
					}
					if (view.Count == 0)
					{
						view.Add("...");
					}
					autoSdkConfig = new AutoSdkConfig(view);
				}
				return autoSdkConfig;
			}
		}
	}
}
