// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Pools;
using EpicGames.Horde.Agents.Sessions;
using Horde.Server.Auditing;

namespace Horde.Server.Agents
{
	/// <summary>
	/// Interface for a collection of agent documents
	/// </summary>
	public interface IAgentCollection
	{
		/// <summary>
		/// Adds a new agent with the given properties
		/// </summary>
		/// <param name="id">Id for the new agent</param>
		/// <param name="ephemeral">Whether the agent is ephemeral or not</param>
		/// <param name="enrollmentKey">Key used to identify a unique enrollment for the agent with this id</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task<IAgent> AddAsync(AgentId id, bool ephemeral, string enrollmentKey, CancellationToken cancellationToken = default);

		/// <summary>
		/// Resets an agent to use new settings
		/// </summary>
		/// <param name="agent">The agent to reset</param>
		/// <param name="ephemeral">Whether the agent is ephemeral or not</param>
		/// <param name="enrollmentKey">Key used to identify a unique enrollment for the agent with this id</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task<IAgent?> TryResetAsync(IAgent agent, bool ephemeral, string enrollmentKey, CancellationToken cancellationToken = default);

		/// <summary>
		/// Deletes an agent
		/// </summary>
		/// <param name="agent">Deletes the agent</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Async task</returns>
		Task<IAgent?> TryDeleteAsync(IAgent agent, CancellationToken cancellationToken = default);

		/// <summary>
		/// Deletes an agent
		/// </summary>
		/// <param name="agentId">Unique id of the agent</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Async task</returns>
		Task ForceDeleteAsync(AgentId agentId, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets an agent by ID
		/// </summary>
		/// <param name="agentId">Unique id of the agent</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The agent document</returns>
		Task<IAgent?> GetAsync(AgentId agentId, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets multiple agents by ID
		/// </summary>
		/// <param name="agentIds">List of unique IDs of the agents</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The agent documents</returns>
		Task<IReadOnlyList<IAgent>> GetManyAsync(List<AgentId> agentIds, CancellationToken cancellationToken = default);

		/// <summary>
		/// Finds all agents matching certain criteria
		/// </summary>
		/// <param name="poolId">The pool ID in string form containing the agent</param>
		/// <param name="modifiedAfter">If set, only returns agents modified after this time</param>
		/// <param name="property">Property to look for</param>
		/// <param name="status">Status to look for</param>
		/// <param name="enabled">Enabled/disabled status to look for</param>
		/// <param name="includeDeleted">Whether agents marked as deleted should be included</param>
		/// <param name="index">Index of the first result</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of agents matching the given criteria</returns>
		Task<IReadOnlyList<IAgent>> FindAsync(PoolId? poolId = null, DateTime? modifiedAfter = null, string? property = null, AgentStatus? status = null, bool? enabled = null, bool includeDeleted = false, int? index = null, int? count = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Finds all agents with an expired session
		/// </summary>
		/// <param name="utcNow">The current time</param>
		/// <param name="maxAgents">Maximum number of agents to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of agents</returns>
		Task<IReadOnlyList<IAgent>> FindExpiredAsync(DateTime utcNow, int maxAgents, CancellationToken cancellationToken = default);

		/// <summary>
		/// Finds all agents marked as deleted
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of agents</returns>
		Task<IReadOnlyList<IAgent>> FindDeletedAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Finds all active agent lease IDs
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of agent lease IDs</returns>
		Task<List<LeaseId>> FindActiveLeaseIdsAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Get all child lease IDs
		/// </summary>
		/// <param name="id">Lease ID</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of agent lease IDs</returns>
		Task<List<LeaseId>> GetChildLeaseIdsAsync(LeaseId id, CancellationToken cancellationToken = default);

		/// <summary>
		/// Update an agent's settings
		/// </summary>
		/// <param name="agent">Agent instance</param>
		/// <param name="enabled">Whether the agent is enabled or not</param>
		/// <param name="requestConform">Whether to request a conform job be run</param>
		/// <param name="requestFullConform">Whether to request a full conform job be run</param>
		/// <param name="requestRestart">Whether to request the machine be restarted</param>
		/// <param name="requestShutdown">Whether to request the machine be shut down</param>
		/// <param name="requestForceRestart">Request an immediate restart without waiting for leases to complete</param>
		/// <param name="shutdownReason">The reason for shutting down agent, ex. Autoscaler/Manual/Unexpected</param>
		/// <param name="pools">List of pools for the agent</param>
		/// <param name="comment">New comment</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>New agent state if update was successful</returns>
		Task<IAgent?> TryUpdateSettingsAsync(IAgent agent, bool? enabled = null, bool? requestConform = null, bool? requestFullConform = null, bool? requestRestart = null, bool? requestShutdown = null, bool? requestForceRestart = null, string? shutdownReason = null, List<PoolId>? pools = null, string? comment = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Update the current workspaces for an agent.
		/// </summary>
		/// <param name="agent">The agent to update</param>
		/// <param name="workspaces">Current list of workspaces</param>
		/// <param name="requestConform">Whether the agent still needs to run another conform</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>New agent state</returns>
		Task<IAgent?> TryUpdateWorkspacesAsync(IAgent agent, List<AgentWorkspaceInfo> workspaces, bool requestConform, CancellationToken cancellationToken = default);

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
		/// <param name="lastStatusChange">Time to force status change timestamp to</param>
		/// <param name="version">Current version of the agent software</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>New agent state</returns>
		Task<IAgent?> TryStartSessionAsync(IAgent agent, SessionId sessionId, DateTime sessionExpiresAt, AgentStatus status, IReadOnlyList<string> properties, IReadOnlyDictionary<string, int> resources, IReadOnlyList<PoolId> pools, IReadOnlyList<PoolId> dynamicPools, DateTime lastStatusChange, string? version, CancellationToken cancellationToken = default);

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
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>True if the document was updated, false if another writer updated the document first</returns>
		Task<IAgent?> TryUpdateSessionAsync(IAgent agent, AgentStatus? status, DateTime? sessionExpiresAt, IReadOnlyList<string>? properties, IReadOnlyDictionary<string, int>? resources, IReadOnlyList<PoolId>? dynamicPools, List<AgentLease>? leases, CancellationToken cancellationToken = default);

		/// <summary>
		/// Terminates the current session
		/// </summary>
		/// <param name="agent">The agent to terminate the session for</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>New agent state if it succeeded, otherwise null</returns>
		Task<IAgent?> TryTerminateSessionAsync(IAgent agent, CancellationToken cancellationToken = default);

		/// <summary>
		/// Attempts to add a lease to an agent
		/// </summary>
		/// <param name="agent">The agent to update</param>
		/// <param name="newLease">The new lease document</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>New agent state if it succeeded, otherwise null</returns>
		Task<IAgent?> TryAddLeaseAsync(IAgent agent, AgentLease newLease, CancellationToken cancellationToken = default);

		/// <summary>
		/// Attempts to cancel a lease
		/// </summary>
		/// <param name="agent">The agent to update</param>
		/// <param name="leaseIdx">Index of the lease</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>New agent state if it succeeded, otherwise null</returns>
		Task<IAgent?> TryCancelLeaseAsync(IAgent agent, int leaseIdx, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets the log channel for an agent
		/// </summary>
		/// <param name="agentId"></param>
		/// <returns></returns>
		IAuditLogChannel<AgentId> GetLogger(AgentId agentId);

		/// <summary>
		/// Sends a notification that an update event has ocurred
		/// </summary>
		/// <param name="agentId">Agent that has been updated</param>
		Task PublishUpdateEventAsync(AgentId agentId);

		/// <summary>
		/// Subscribe to notifications on agent states being updated
		/// </summary>
		/// <param name="onUpdate">Callback for updates</param>
		/// <returns>Disposable subscription object</returns>
		Task<IAsyncDisposable> SubscribeToUpdateEventsAsync(Action<AgentId> onUpdate);
	}
}
