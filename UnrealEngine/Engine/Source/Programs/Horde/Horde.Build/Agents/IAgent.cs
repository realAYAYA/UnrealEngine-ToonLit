// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Cryptography;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Common;
using EpicGames.Horde.Compute;
using Google.Protobuf.WellKnownTypes;
using Horde.Build.Acls;
using Horde.Build.Agents.Leases;
using Horde.Build.Agents.Pools;
using Horde.Build.Agents.Sessions;
using Horde.Build.Agents.Software;
using Horde.Build.Logs;
using Horde.Build.Perforce;
using Horde.Build.Server;
using Horde.Build.Streams;
using Horde.Build.Utilities;
using HordeCommon;
using HordeCommon.Rpc.Tasks;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;

namespace Horde.Build.Agents
{
	using AgentSoftwareChannelName = StringId<AgentSoftwareChannels>;
	using LeaseId = ObjectId<ILease>;
	using LogId = ObjectId<ILogFile>;
	using PoolId = StringId<IPool>;
	using SessionId = ObjectId<ISession>;
	using StreamId = StringId<IStream>;

	/// <summary>
	/// Information about a workspace synced to an agent
	/// </summary>
	public class AgentWorkspace
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
		public bool BIncremental { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="cluster">Name of the Perforce cluster</param>
		/// <param name="userName">User to log into Perforce with (eg. buildmachine)</param>
		/// <param name="identifier">Identifier to distinguish this workspace from other workspaces</param>
		/// <param name="stream">The stream to sync</param>
		/// <param name="view">Custom view for the workspace</param>
		/// <param name="bIncremental">Whether to use an incremental workspace</param>
		public AgentWorkspace(string? cluster, string? userName, string identifier, string stream, List<string>? view, bool bIncremental)
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
			BIncremental = bIncremental;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="workspace">RPC message to construct from</param>
		public AgentWorkspace(HordeCommon.Rpc.Messages.AgentWorkspace workspace)
			: this(workspace.ConfiguredCluster, workspace.ConfiguredUserName, workspace.Identifier, workspace.Stream, (workspace.View.Count > 0) ? workspace.View.ToList() : null, workspace.Incremental)
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
			AgentWorkspace? other = obj as AgentWorkspace;
			if (other == null)
			{
				return false;
			}
			if (Cluster != other.Cluster || UserName != other.UserName || Identifier != other.Identifier || Stream != other.Stream || BIncremental != other.BIncremental)
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
			return HashCode.Combine(Cluster, UserName, Identifier, Stream, BIncremental); // Ignore 'View' for now
		}

		/// <summary>
		/// Checks if two workspace sets are equivalent, ignoring order
		/// </summary>
		/// <param name="workspacesA">First list of workspaces</param>
		/// <param name="workspacesB">Second list of workspaces</param>
		/// <returns>True if the sets are equivalent</returns>
		public static bool SetEquals(IReadOnlyList<AgentWorkspace> workspacesA, IReadOnlyList<AgentWorkspace> workspacesB)
		{
			HashSet<AgentWorkspace> workspacesSetA = new HashSet<AgentWorkspace>(workspacesA);
			return workspacesSetA.SetEquals(workspacesB);
		}

		/// <summary>
		/// Converts this workspace to an RPC message
		/// </summary>
		/// <param name="server">The Perforce server</param>
		/// <param name="credentials">Credentials for the server</param>
		/// <returns>The RPC message</returns>
		public HordeCommon.Rpc.Messages.AgentWorkspace ToRpcMessage(IPerforceServer server, PerforceCredentials? credentials)
		{
			// Construct the message
			HordeCommon.Rpc.Messages.AgentWorkspace result = new HordeCommon.Rpc.Messages.AgentWorkspace();
			result.ConfiguredCluster = Cluster;
			result.ConfiguredUserName = UserName;
			result.ServerAndPort = server.ServerAndPort;
			result.UserName = credentials?.UserName ?? UserName;
			result.Password = credentials?.Password;
			result.Identifier = Identifier;
			result.Stream = Stream;
			if (View != null)
			{
				result.View.AddRange(View);
			}
			result.Incremental = BIncremental;
			return result;
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
		/// <param name="name">Name of this lease</param>
		/// <param name="streamId"></param>
		/// <param name="poolId"></param>
		/// <param name="logId">Unique id for the log</param>
		/// <param name="state">State for the lease</param>
		/// <param name="resources">Resources required for this lease</param>
		/// <param name="exclusive">Whether to reserve the entire device</param>
		/// <param name="payload">Encoded "any" protobuf describing the contents of the payload</param>
		public AgentLease(LeaseId id, string name, StreamId? streamId, PoolId? poolId, LogId? logId, LeaseState state, IReadOnlyDictionary<string, int>? resources, bool exclusive, byte[]? payload)
		{
			Id = id;
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
			lease.State = State;
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
	}

	/// <summary>
	/// Mirrors an Agent document in the database
	/// </summary>
	public interface IAgent
	{
		/// <summary>
		/// Randomly generated unique id for this agent.
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
		/// Whether the agent is enabled
		/// </summary>
		public bool Enabled { get; }

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
		/// Channel for the software running on this agent. Uses <see cref="AgentSoftwareService.DefaultChannelName"/> if not specified
		/// </summary>
		public AgentSoftwareChannelName? Channel { get; }

		/// <summary>
		/// Last upgrade that was attempted
		/// </summary>
		public string? LastUpgradeVersion { get; }

		/// <summary>
		/// Time that which the last upgrade was attempted
		/// </summary>
		public DateTime? LastUpgradeTime { get; }

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
		/// The reason for the last agent shutdown
		/// </summary>
		public string? LastShutdownReason { get; }

		/// <summary>
		/// List of workspaces currently synced to this machine
		/// </summary>
		public IReadOnlyList<AgentWorkspace> Workspaces { get; }

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
		/// ACL for modifying this agent
		/// </summary>
		public Acl? Acl { get; }

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
		/// <returns>True if the new lease can be granted</returns>
		public static bool MeetsRequirements(this IAgent agent, Requirements requirements)
		{
			return MeetsRequirements(agent, requirements.Condition, requirements.Resources, requirements.Exclusive);
		}

		/// <summary>
		/// Determine whether it's possible to add a lease for the given resources
		/// </summary>
		/// <param name="agent">The agent to create a lease for</param>
		/// <param name="exclusive">Whether t</param>
		/// <param name="condition">Condition to satisfy</param>
		/// <param name="resources">Resources required to execute</param>
		/// <returns>True if the new lease can be granted</returns>
		public static bool MeetsRequirements(this IAgent agent, Condition? condition, Dictionary<string, int>? resources, bool exclusive)
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
			if (condition != null && !agent.SatisfiesCondition(condition))
			{
				return false;
			}
			if (resources != null)
			{
				foreach ((string name, int count) in resources)
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
					if (remainingCount < count)
					{
						return false;
					}
				}
			}
			return true;
		}

		/// <summary>
		/// Gets all the autosdk workspaces required for an agent
		/// </summary>
		/// <param name="agent"></param>
		/// <param name="globals"></param>
		/// <param name="workspaces"></param>
		/// <returns></returns>
		public static HashSet<AgentWorkspace> GetAutoSdkWorkspaces(this IAgent agent, Globals globals, List<AgentWorkspace> workspaces)
		{
			HashSet<AgentWorkspace> autoSdkWorkspaces = new HashSet<AgentWorkspace>();
			foreach (string? clusterName in workspaces.Select(x => x.Cluster).Distinct())
			{
				PerforceCluster? cluster = globals.FindPerforceCluster(clusterName);
				if (cluster != null)
				{
					AgentWorkspace? autoSdkWorkspace = GetAutoSdkWorkspace(agent, cluster);
					if (autoSdkWorkspace != null)
					{
						autoSdkWorkspaces.Add(autoSdkWorkspace);
					}
				}
			}
			return autoSdkWorkspaces;
		}

		/// <summary>
		/// Get the AutoSDK workspace required for an agent
		/// </summary>
		/// <param name="agent"></param>
		/// <param name="cluster">The perforce cluster to get a workspace for</param>
		/// <returns></returns>
		public static AgentWorkspace? GetAutoSdkWorkspace(this IAgent agent, PerforceCluster cluster)
		{
			foreach (AutoSdkWorkspace autoSdk in cluster.AutoSdk)
			{
				if (autoSdk.Stream != null && autoSdk.Properties.All(x => agent.Properties.Contains(x)))
				{
					return new AgentWorkspace(cluster.Name, autoSdk.UserName, autoSdk.Name ?? "AutoSDK", autoSdk.Stream!, null, true);
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
		/// <returns>The RPC message</returns>
		public static async Task<bool> TryAddWorkspaceMessage(this IAgent agent, AgentWorkspace workspace, PerforceCluster cluster, PerforceLoadBalancer loadBalancer, IList<HordeCommon.Rpc.Messages.AgentWorkspace> workspaceMessages)
		{
			// Find a matching server, trying to use a previously selected one if possible
			string? baseServerAndPort;
			string? serverAndPort;

			HordeCommon.Rpc.Messages.AgentWorkspace? existingWorkspace = workspaceMessages.FirstOrDefault(x => x.ConfiguredCluster == workspace.Cluster);
			if(existingWorkspace != null)
			{
				baseServerAndPort = existingWorkspace.BaseServerAndPort;
				serverAndPort = existingWorkspace.ServerAndPort;
			}
			else
			{
				if (cluster == null)
				{
					return false;
				}

				IPerforceServer? server = await loadBalancer.SelectServerAsync(cluster, agent);
				if (server == null)
				{
					return false;
				}

				baseServerAndPort = server.BaseServerAndPort;
				serverAndPort = server.ServerAndPort;
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
			HordeCommon.Rpc.Messages.AgentWorkspace result = new HordeCommon.Rpc.Messages.AgentWorkspace();
			result.ConfiguredCluster = workspace.Cluster;
			result.ConfiguredUserName = workspace.UserName;
			result.Cluster = cluster?.Name;
			result.BaseServerAndPort = baseServerAndPort;
			result.ServerAndPort = serverAndPort;
			result.UserName = credentials?.UserName ?? workspace.UserName;
			result.Password = credentials?.Password;
			result.Identifier = workspace.Identifier;
			result.Stream = workspace.Stream;
			if (workspace.View != null)
			{
				result.View.AddRange(workspace.View);
			}
			result.Incremental = workspace.BIncremental;
			workspaceMessages.Add(result);

			return true;
		}
	}
}
