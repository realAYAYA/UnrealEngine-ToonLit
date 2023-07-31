// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Build.Agents.Pools;
using Horde.Build.Agents.Sessions;
using Horde.Build.Logs;
using Horde.Build.Streams;
using Horde.Build.Utilities;
using HordeCommon;

namespace Horde.Build.Agents.Leases
{
	using LeaseId = ObjectId<ILease>;
	using LogId = ObjectId<ILogFile>;
	using PoolId = StringId<IPool>;
	using SessionId = ObjectId<ISession>;
	using StreamId = StringId<IStream>;

	/// <summary>
	/// Interface for a collection of lease documents
	/// </summary>
	public interface ILeaseCollection
	{
		/// <summary>
		/// Adds a lease to the collection
		/// </summary>
		/// <param name="id">The lease id</param>
		/// <param name="name">Name of the lease</param>
		/// <param name="agentId">The agent id</param>
		/// <param name="sessionId">The agent session handling the lease</param>
		/// <param name="streamId">Stream for the payload</param>
		/// <param name="poolId">The pool for the lease</param>
		/// <param name="logId">Log id for the lease</param>
		/// <param name="startTime">Start time of the lease</param>
		/// <param name="payload">Payload for the lease</param>
		/// <returns>Async task</returns>
		Task<ILease> AddAsync(LeaseId id, string name, AgentId agentId, SessionId sessionId, StreamId? streamId, PoolId? poolId, LogId? logId, DateTime startTime, byte[] payload);

		/// <summary>
		/// Deletes a lease from the collection
		/// </summary>
		/// <param name="leaseId">Unique id of the lease</param>
		/// <returns>Async task</returns>
		Task DeleteAsync(LeaseId leaseId);

		/// <summary>
		/// Gets a specific lease
		/// </summary>
		/// <param name="leaseId">Unique id of the lease</param>
		/// <returns>The lease that was found, or null if it does not exist</returns>
		Task<ILease?> GetAsync(LeaseId leaseId);

		/// <summary>
		/// Finds all leases matching a set of criteria
		/// </summary>
		/// <param name="agentId">Unqiue id of the agent executing this lease</param>
		/// <param name="sessionId">Unique id of the agent session</param>
		/// <param name="minTime">Start of the window to include leases</param>
		/// <param name="maxTime">End of the window to include leases</param>
		/// <param name="index">Index of the first result to return</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="indexHint">Name of index to be specified as a hint to the database query planner</param>
		/// <param name="consistentRead">If the database read should be made to the replica server</param>
		/// <returns>List of leases matching the given criteria</returns>
		Task<List<ILease>> FindLeasesAsync(AgentId? agentId = null, SessionId? sessionId = null, DateTime? minTime = null, DateTime? maxTime = null, int? index = null, int? count = null, string? indexHint = null, bool consistentRead = true);

		/// <summary>
		/// Finds all leases by finish time
		/// </summary>
		/// <param name="minFinishTime">Start of finish time window</param>
		/// <param name="maxFinishTime">End of finish time window</param>
		/// <param name="index">Index of the first result to return</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="indexHint">Name of index to be specified as a hint to the database query planner</param>
		/// <param name="consistentRead">If the database read should be made to the replica server</param>
		/// <returns>List of leases matching the given criteria</returns>
		Task<List<ILease>> FindLeasesByFinishTimeAsync(DateTime? minFinishTime, DateTime? maxFinishTime, int? index, int? count, string? indexHint = null, bool consistentRead = true);

		/// <summary>
		/// Finds all leases between min and/or max time
		/// Preferred over the more generic FindLeasesAsync as this one use a better index and relaxed read consistency
		/// </summary>
		/// <param name="minTime">Start of the window to include leases</param>
		/// <param name="maxTime">End of the window to include leases</param>
		/// <returns>List of leases matching the given criteria</returns>
		Task<List<ILease>> FindLeasesAsync(DateTime? minTime, DateTime? maxTime);

		/// <summary>
		/// Finds all active leases
		/// </summary>
		/// <param name="index">Index of the first result to return</param>
		/// <param name="count">Number of results to return</param>
		/// <returns>List of leases</returns>
		Task<List<ILease>> FindActiveLeasesAsync(int? index = null, int? count = null);

		/// <summary>
		/// Sets the outcome of a lease
		/// </summary>
		/// <param name="leaseId">The lease to update</param>
		/// <param name="finishTime">Time at which the lease finished</param>
		/// <param name="outcome">Outcome of the lease</param>
		/// <param name="output">Output data from the task</param>
		/// <returns>True if the lease was updated, false otherwise</returns>
		Task<bool> TrySetOutcomeAsync(LeaseId leaseId, DateTime finishTime, LeaseOutcome outcome, byte[]? output);
	}
}
