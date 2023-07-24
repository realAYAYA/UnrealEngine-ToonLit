// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Amazon.Util.Internal;
using EpicGames.Horde.Common;
using Horde.Build.Agents.Fleet;
using Horde.Build.Utilities;

namespace Horde.Build.Agents.Pools
{
	using PoolId = StringId<IPool>;

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
		/// <param name="condition">Condition for agents to be included in this pool</param>
		/// <param name="enableAutoscaling">Whether to enable autoscaling for this pool</param>
		/// <param name="minAgents">Minimum number of agents in the pool</param>
		/// <param name="numReserveAgents">Minimum number of idle agents to maintain</param>
		/// <param name="conformInterval">Interval between conforms. Set to zero to disable.</param>
		/// <param name="scaleOutCooldown">Cooldown time between scale-out events</param>
		/// <param name="scaleInCooldown">Cooldown time between scale-in events</param>
		/// <param name="sizeStrategies">Pool sizing strategies</param>
		/// <param name="fleetManagers">Fleet managers</param>
		/// <param name="sizeStrategy">Pool sizing strategy</param>
		/// <param name="leaseUtilizationSettings">Settings for lease utilization strategy</param>
		/// <param name="jobQueueSettings">Settings for job queue strategy</param>
		/// <param name="computeQueueAwsMetricSettings">Settings for compute queue AWS metric strategy</param>
		/// <param name="properties">Properties for the pool</param>
		/// <returns>The new pool document</returns>
		Task<IPool> AddAsync(
			PoolId id,
			string name,
			Condition? condition = null,
			bool? enableAutoscaling = null,
			int? minAgents = null,
			int? numReserveAgents = null,
			TimeSpan? conformInterval = null,
			TimeSpan? scaleOutCooldown = null,
			TimeSpan? scaleInCooldown = null,
			List<PoolSizeStrategyInfo>? sizeStrategies = null,
			List<FleetManagerInfo>? fleetManagers = null,
			PoolSizeStrategy? sizeStrategy = null,
			LeaseUtilizationSettings? leaseUtilizationSettings = null,
			JobQueueSettings? jobQueueSettings = null,
			ComputeQueueAwsMetricSettings? computeQueueAwsMetricSettings = null,
			IEnumerable<KeyValuePair<string, string>>? properties = null);

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
		/// <param name="newName">New name for the pool</param>
		/// <param name="newCondition">New condition for the pool</param>
		/// <param name="newEnableAutoscaling">New setting for whether to enable autoscaling</param>
		/// <param name="newMinAgents">Minimum number of agents in the pool</param>
		/// <param name="newNumReserveAgents">Minimum number of idle agents to maintain</param>
		/// <param name="newWorkspaces">New workspaces for the pool</param>
		/// <param name="newUseAutoSdk">New setting for whether to use autosdk</param>
		/// <param name="newProperties">New properties for the pool</param>
		/// <param name="conformInterval">Interval between conforms. Set to zero to disable.</param>
		/// <param name="lastScaleUpTime">New time for last (auto) scale up</param>
		/// <param name="lastScaleDownTime">New time for last (auto) scale down</param>
		/// <param name="scaleOutCooldown">Cooldown time between scale-out events</param>
		/// <param name="scaleInCooldown">Cooldown time between scale-in events</param>
		/// <param name="sizeStrategy">Pool sizing strategy</param>
		/// <param name="newSizeStrategies">List of pool sizing strategies</param>
		/// <param name="newFleetManagers">List of fleet managers</param>
		/// <param name="leaseUtilizationSettings">Settings for lease utilization strategy</param>
		/// <param name="jobQueueSettings">Settings for job queue strategy</param>
		/// <param name="computeQueueAwsMetricSettings">Settings for compute queue AWS metric strategy</param>
		/// <param name="useDefaultStrategy">Whether to use the default strategy</param>
		/// <returns>Async task</returns>
		Task<IPool?> TryUpdateAsync(
			IPool pool,
			string? newName = null,
			Condition? newCondition = null,
			bool? newEnableAutoscaling = null,
			int? newMinAgents = null,
			int? newNumReserveAgents = null,
			List<AgentWorkspace>? newWorkspaces = null,
			bool? newUseAutoSdk = null,
			Dictionary<string, string?>? newProperties = null,
			TimeSpan? conformInterval = null,
			DateTime? lastScaleUpTime = null,
			DateTime? lastScaleDownTime = null,
			TimeSpan? scaleOutCooldown = null,
			TimeSpan? scaleInCooldown = null,
			PoolSizeStrategy? sizeStrategy = null,
			List<PoolSizeStrategyInfo>? newSizeStrategies = null,
			List<FleetManagerInfo>? newFleetManagers = null,
			LeaseUtilizationSettings? leaseUtilizationSettings = null,
			JobQueueSettings? jobQueueSettings = null,
			ComputeQueueAwsMetricSettings? computeQueueAwsMetricSettings = null,
			bool? useDefaultStrategy = null);

		/// <summary>
		/// Updates the list of pools from a config file
		/// </summary>
		/// <param name="poolConfigs">Configuration for the pools, and revision string for the config file containing them</param>
		Task ConfigureAsync(IReadOnlyList<(PoolConfig Config, string Revision)> poolConfigs);
	}
}
