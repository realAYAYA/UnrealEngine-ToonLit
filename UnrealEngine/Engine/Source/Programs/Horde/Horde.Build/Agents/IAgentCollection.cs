// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Build.Acls;
using Horde.Build.Agents.Pools;
using Horde.Build.Agents.Sessions;
using Horde.Build.Agents.Software;
using Horde.Build.Auditing;
using Horde.Build.Utilities;
using HordeCommon;

namespace Horde.Build.Agents
{
	using AgentSoftwareChannelName = StringId<AgentSoftwareChannels>;
	using PoolId = StringId<IPool>;
	using SessionId = ObjectId<ISession>;

	/// <summary>
	/// Interface for a collection of agent documents
	/// </summary>
	public interface IAgentCollection
	{
		/// <summary>
		/// Adds a new agent with the given properties
		/// </summary>
		/// <param name="id">Id for the new agent</param>
		/// <param name="bEnabled">Whether the agent is enabled or not</param>
		/// <param name="channel">Channel to use for software run by this agent</param>
		/// <param name="pools">Pools for the agent</param>
		Task<IAgent> AddAsync(AgentId id, bool bEnabled, AgentSoftwareChannelName? channel = null, List<PoolId>? pools = null);

		/// <summary>
		/// Deletes an agent
		/// </summary>
		/// <param name="agent">Deletes the agent</param>
		/// <returns>Async task</returns>
		Task<IAgent?> TryDeleteAsync(IAgent agent);

		/// <summary>
		/// Deletes an agent
		/// </summary>
		/// <param name="agentId">Unique id of the agent</param>
		/// <returns>Async task</returns>
		Task ForceDeleteAsync(AgentId agentId);

		/// <summary>
		/// Gets an agent by ID
		/// </summary>
		/// <param name="agentId">Unique id of the agent</param>
		/// <returns>The agent document</returns>
		Task<IAgent?> GetAsync(AgentId agentId);

		/// <summary>
		/// Finds all agents matching certain criteria
		/// </summary>
		/// <param name="poolId">The pool ID in string form containing the agent</param>
		/// <param name="modifiedAfter">If set, only returns agents modified after this time</param>
		/// <param name="status">Status to look for</param>
		/// <param name="enabled">Enabled/disabled status to look for</param>
		/// <param name="index">Index of the first result</param>
		/// <param name="count">Number of results to return</param>
		/// <returns>List of agents matching the given criteria</returns>
		Task<List<IAgent>> FindAsync(PoolId? poolId = null, DateTime? modifiedAfter = null, AgentStatus? status = null, bool? enabled = null, int? index = null, int? count = null);

		/// <summary>
		/// Finds all the expried agents
		/// </summary>
		/// <param name="utcNow">The current time</param>
		/// <param name="maxAgents">Maximum number of agents to return</param>
		/// <returns>List of agents</returns>
		Task<List<IAgent>> FindExpiredAsync(DateTime utcNow, int maxAgents);

		/// <summary>
		/// Update an agent's settings
		/// </summary>
		/// <param name="agent">Agent instance</param>
		/// <param name="bEnabled">Whether the agent is enabled or not</param>
		/// <param name="bRequestConform">Whether to request a conform job be run</param>
		/// <param name="bRequestFullConform">Whether to request a full conform job be run</param>
		/// <param name="bRequestRestart">Whether to request the machine be restarted</param>
		/// <param name="bRequestShutdown">Whether to request the machine be shut down</param>
		/// <param name="shutdownReason">The reason for shutting down agent, ex. Autoscaler/Manual/Unexpected</param>
		/// <param name="channel">Override for the desired software channel</param>
		/// <param name="pools">List of pools for the agent</param>
		/// <param name="acl">New ACL for this agent</param>
		/// <param name="comment">New comment</param>
		/// <returns>Version of the software that needs to be installed on the agent. Null if the agent is running the correct version.</returns>
		Task<IAgent?> TryUpdateSettingsAsync(IAgent agent, bool? bEnabled = null, bool? bRequestConform = null, bool? bRequestFullConform = null, bool? bRequestRestart = null, bool? bRequestShutdown = null, string? shutdownReason = null, AgentSoftwareChannelName? channel = null, List<PoolId>? pools = null, Acl? acl = null, string? comment = null);

		/// <summary>
		/// Update the current workspaces for an agent.
		/// </summary>
		/// <param name="agent">The agent to update</param>
		/// <param name="workspaces">Current list of workspaces</param>
		/// <param name="requestConform">Whether the agent still needs to run another conform</param>
		/// <returns>New agent state</returns>
		Task<IAgent?> TryUpdateWorkspacesAsync(IAgent agent, List<AgentWorkspace> workspaces, bool requestConform);

		/// <summary>
		/// Sets the current session
		/// </summary>
		/// <param name="agent">The agent to update</param>
		/// <param name="sessionId">New session id</param>
		/// <param name="sessionExpiresAt">Expiry time for the new session</param>
		/// <param name="status">Status of the agent</param>
		/// <param name="properties">Properties for the current session</param>
		/// <param name="resources">Resources for the agent</param>
		/// <param name="pools">New list of pools for the agent</param>
		/// <param name="dynamicPools">New list of dynamic pools for the agent</param>
		/// <param name="version">Current version of the agent software</param>
		/// <returns>New agent state</returns>
		Task<IAgent?> TryStartSessionAsync(IAgent agent, SessionId sessionId, DateTime sessionExpiresAt, AgentStatus status, IReadOnlyList<string> properties, IReadOnlyDictionary<string, int> resources, IReadOnlyList<PoolId> pools, IReadOnlyList<PoolId> dynamicPools, string? version);

		/// <summary>
		/// Attempt to update the agent state
		/// </summary>
		/// <param name="agent">Agent instance</param>
		/// <param name="status">New status of the agent</param>
		/// <param name="sessionExpiresAt">New expiry time for the current session</param>
		/// <param name="properties">Properties for the current session</param>
		/// <param name="resources">Resources for the agent</param>
		/// <param name="dynamicPools">New list of dynamic pools for the agent</param>
		/// <param name="leases">New set of leases</param>
		/// <returns>True if the document was updated, false if another writer updated the document first</returns>
		Task<IAgent?> TryUpdateSessionAsync(IAgent agent, AgentStatus? status, DateTime? sessionExpiresAt, IReadOnlyList<string>? properties, IReadOnlyDictionary<string, int>? resources, IReadOnlyList<PoolId>? dynamicPools, List<AgentLease>? leases);

		/// <summary>
		/// Terminates the current session
		/// </summary>
		/// <param name="agent">The agent to terminate the session for</param>
		/// <returns>New agent state if it succeeded, otherwise null</returns>
		Task<IAgent?> TryTerminateSessionAsync(IAgent agent);

		/// <summary>
		/// Attempts to add a lease to an agent
		/// </summary>
		/// <param name="agent">The agent to update</param>
		/// <param name="newLease">The new lease document</param>
		/// <returns>New agent state if it succeeded, otherwise null</returns>
		Task<IAgent?> TryAddLeaseAsync(IAgent agent, AgentLease newLease);

		/// <summary>
		/// Attempts to cancel a lease
		/// </summary>
		/// <param name="agent">The agent to update</param>
		/// <param name="leaseIdx">Index of the lease</param>
		/// <returns>New agent state if it succeeded, otherwise null</returns>
		Task<IAgent?> TryCancelLeaseAsync(IAgent agent, int leaseIdx);

		/// <summary>
		/// Gets the log channel for an agent
		/// </summary>
		/// <param name="agentId"></param>
		/// <returns></returns>
		IAuditLogChannel<AgentId> GetLogger(AgentId agentId);

		/// <summary>
		/// Subscribe to notifications on agent states being updated
		/// </summary>
		/// <param name="onUpdate">Callback for updates</param>
		/// <returns>Disposable subscription object</returns>
		Task<IDisposable> SubscribeToUpdateEventsAsync(Action<AgentId> onUpdate);
	}
}
