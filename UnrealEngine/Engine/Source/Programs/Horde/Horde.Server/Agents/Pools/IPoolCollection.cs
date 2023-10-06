// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Common;
using Horde.Server.Agents.Fleet;

namespace Horde.Server.Agents.Pools
{
	/// <summary>
	/// Collection of pool documents
	/// </summary>
	public interface IPoolCollection
	{
		/// <summary>
		/// Creates a new pool
		/// </summary>
		/// <param name="id">Unique id for the new pool</param>
		/// <param name="name">Name of the new pool</param>
		/// <param name="options">Options for the new pool</param>
		/// <returns>The new pool document</returns>
		Task<IPool> AddAsync(PoolId id, string name, AddPoolOptions options);

		/// <summary>
		/// Enumerates all the pools
		/// </summary>
		/// <returns>The pool documents</returns>
		Task<List<IPool>> GetAsync();

		/// <summary>
		/// Gets a pool by ID
		/// </summary>
		/// <param name="id">Unique id of the pool</param>
		/// <returns>The pool document</returns>
		Task<IPool?> GetAsync(PoolId id);

		/// <summary>
		/// Gets a list of all valid pool ids
		/// </summary>
		/// <returns>List of pool ids</returns>
		Task<List<PoolId>> GetPoolIdsAsync();

		/// <summary>
		/// Gets a pool by ID
		/// </summary>
		/// <param name="id">Unique id of the pool</param>
		/// <returns>The pool document</returns>
		Task<bool> DeleteAsync(PoolId id);

		/// <summary>
		/// Updates a pool
		/// </summary>
		/// <param name="pool">The pool to update</param>
		/// <param name="options">Options for the update</param>
		Task<IPool?> TryUpdateAsync(IPool pool, UpdatePoolOptions options);

		/// <summary>
		/// Updates the list of pools from a config file
		/// </summary>
		/// <param name="poolConfigs">Configuration for the pools, and revision string for the config file containing them</param>
		Task ConfigureAsync(IReadOnlyList<(PoolConfig Config, string Revision)> poolConfigs);
	}

	/// <summary>
	/// Settings for creating a new pool
	/// </summary>
	public class AddPoolOptions
	{
		/// <summary>
		/// Condition for agents to be included in this pool
		/// </summary>
		public Condition? Condition { get; set; }

		/// <summary>
		/// Whether to enable autoscaling for this pool
		/// </summary>
		public bool? EnableAutoscaling { get; set; }

		/// <summary>
		/// Minimum number of agents in the pool
		/// </summary>
		public int? MinAgents { get; set; }

		/// <summary>
		/// Minimum number of idle agents to maintain
		/// </summary>
		public int? NumReserveAgents { get; set; }

		/// <summary>
		/// Interval between conforms. Set to zero to disable.
		/// </summary>
		public TimeSpan? ConformInterval { get; set; }

		/// <summary>
		/// Cooldown time between scale-out events
		/// </summary>
		public TimeSpan? ScaleOutCooldown { get; set; }

		/// <summary>
		/// Cooldown time between scale-in events
		/// </summary>
		public TimeSpan? ScaleInCooldown { get; set; }

		/// <summary>
		/// Pool sizing strategies
		/// </summary>
		public List<PoolSizeStrategyInfo>? SizeStrategies { get; set; }

		/// <summary>
		/// Fleet managers
		/// </summary>
		public List<FleetManagerInfo>? FleetManagers { get; set; }

		/// <summary>
		/// Pool sizing strategy
		/// </summary>
		public PoolSizeStrategy? SizeStrategy { get; set; }

		/// <summary>
		/// Settings for lease utilization strategy
		/// </summary>
		public LeaseUtilizationSettings? LeaseUtilizationSettings { get; set; }

		/// <summary>
		/// Settings for job queue strategy
		/// </summary>
		public JobQueueSettings? JobQueueSettings { get; set; }

		/// <summary>
		/// Settings for compute queue AWS metric strategy
		/// </summary>
		public ComputeQueueAwsMetricSettings? ComputeQueueAwsMetricSettings { get; set; }

		/// <summary>
		/// Properties for the pool
		/// </summary>
		public IEnumerable<KeyValuePair<string, string>>? Properties { get; set; }
	}

	/// <summary>
	/// Settings for updating a pool state
	/// </summary>
	public class UpdatePoolOptions
	{
		/// <summary>
		/// New name for the pool
		/// </summary>
		public string? Name { get; set; }

		/// <summary>
		/// New condition for the pool
		/// </summary>
		public Condition? Condition { get; set; }

		/// <summary>
		/// New setting for whether to enable autoscaling
		/// </summary>
		public bool? EnableAutoscaling { get; set; }

		/// <summary>
		/// Minimum number of agents in the pool
		/// </summary>
		public int? MinAgents { get; set; }

		/// <summary>
		/// Minimum number of idle agents to maintain
		/// </summary>
		public int? NumReserveAgents { get; set; }

		/// <summary>
		/// New workspaces for the pool
		/// </summary>
		public List<AgentWorkspace>? Workspaces { get; set; }

		/// <summary>
		/// Settings for the autosdk workspace
		/// </summary>
		public AutoSdkConfig? AutoSdkConfig { get; set; }

		/// <summary>
		/// New properties for the pool
		/// </summary>
		public Dictionary<string, string?>? Properties { get; set; }

		/// <summary>
		/// Interval between conforms. Set to zero to disable.
		/// </summary>
		public TimeSpan? ConformInterval { get; set; }

		/// <summary>
		/// New time for last (auto) scale up
		/// </summary>
		public DateTime? LastScaleUpTime { get; set; }

		/// <summary>
		/// New time for last (auto) scale down
		/// </summary>
		public DateTime? LastScaleDownTime { get; set; }

		/// <summary>
		/// Cooldown time between scale-out events
		/// </summary>
		public TimeSpan? ScaleOutCooldown { get; set; }

		/// <summary>
		/// Cooldown time between scale-in events
		/// </summary>
		public TimeSpan? ScaleInCooldown { get; set; }

		/// <summary>
		/// Time to wait before shutting down a disabled agent
		/// </summary>
		public TimeSpan? ShutdownIfDisabledGracePeriod { get; set; }

		/// <summary>
		/// Result from last scaling in/out attempt
		/// </summary>
		public ScaleResult? LastScaleResult { get; set; }

		/// <summary>
		/// Last calculated agent count
		/// </summary>
		public int? LastAgentCount { get; set; }

		/// <summary>
		/// Last calculated desired agent count
		/// </summary>
		public int? LastDesiredAgentCount { get; set; }

		/// <summary>
		/// Pool sizing strategy
		/// </summary>
		public PoolSizeStrategy? SizeStrategy { get; set; }

		/// <summary>
		/// List of pool sizing strategies
		/// </summary>
		public List<PoolSizeStrategyInfo>? SizeStrategies { get; set; }

		/// <summary>
		/// List of fleet managers
		/// </summary>
		public List<FleetManagerInfo>? FleetManagers { get; set; }

		/// <summary>
		/// Settings for lease utilization strategy
		/// </summary>
		public LeaseUtilizationSettings? LeaseUtilizationSettings { get; set; }

		/// <summary>
		/// Settings for job queue strategy
		/// </summary>
		public JobQueueSettings? JobQueueSettings { get; set; }

		/// <summary>
		/// Settings for compute queue AWS metric strategy
		/// </summary>
		public ComputeQueueAwsMetricSettings? ComputeQueueAwsMetricSettings { get; set; }

		/// <summary>
		/// Whether to use the default strategy
		/// </summary>
		public bool? UseDefaultStrategy { get; set; }
	}

	/// <summary>
	/// Exception when conflicting definitions for a pool are encountered
	/// </summary>
	public sealed class PoolConflictException : Exception
	{
		/// <summary>
		/// Pool with conflicting definitions
		/// </summary>
		public PoolId PoolId { get; }

		/// <summary>
		/// The first revision string
		/// </summary>
		public string PrevRevision { get; }

		/// <summary>
		/// The second revision string
		/// </summary>
		public string NextRevision { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public PoolConflictException(PoolId poolId, string prevRevision, string nextRevision)
			: base($"Duplicate definitions for {poolId} - first in {prevRevision}, now {nextRevision}")
		{
			PoolId = poolId;
			PrevRevision = prevRevision;
			NextRevision = nextRevision;
		}
	}
}
