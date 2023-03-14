// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Common;
using Horde.Build.Agents.Fleet;
using Horde.Build.Server;
using Horde.Build.Utilities;
using HordeCommon;

namespace Horde.Build.Agents.Pools
{
	using PoolId = StringId<IPool>;

	/// <summary>
	/// Wraps functionality for manipulating pools
	/// </summary>
	public class PoolService
	{
		/// <summary>
		/// The database service instance
		/// </summary>
		readonly MongoService _mongoService;

		/// <summary>
		/// Collection of pool documents
		/// </summary>
		readonly IPoolCollection _pools;

		/// <summary>
		/// Returns the current time
		/// </summary>
		readonly IClock _clock;

		/// <summary>
		/// Cached set of pools, along with the timestamp that it was obtained
		/// </summary>
		Tuple<DateTime, Dictionary<PoolId, IPool>>? _cachedPoolLookup;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService"></param>
		/// <param name="pools">Collection of pool documents</param>
		/// <param name="clock"></param>
		public PoolService(MongoService mongoService, IPoolCollection pools, IClock clock)
		{
			_mongoService = mongoService;
			_pools = pools;
			_clock = clock;
		}

		/// <summary>
		/// Creates a new pool
		/// </summary>
		/// <param name="name">Name of the new pool</param>
		/// <param name="condition">Condition for agents to be automatically included in this pool</param>
		/// <param name="enableAutoscaling">Whether to enable autoscaling for this pool</param>
		/// <param name="minAgents">Minimum number of agents in the pool</param>
		/// <param name="numReserveAgents">Minimum number of idle agents to maintain</param>
		/// <param name="conformInterval">Interval between conforms. Set to zero to disable.</param>
		/// <param name="scaleOutCooldown">Cooldown time between scale-out events</param>
		/// <param name="scaleInCooldown">Cooldown time between scale-in events</param>
		/// <param name="sizeStrategy">Pool sizing strategy</param>
		/// <param name="leaseUtilizationSettings">Settings for lease utilization strategy</param>
		/// <param name="jobQueueSettings">Settings for job queue strategy</param>
		/// <param name="computeQueueAwsMetricSettings">Settings for compute queue AWS metric strategy</param>
		/// <param name="properties">Properties for the new pool</param>
		/// <returns>The new pool document</returns>
		public Task<IPool> CreatePoolAsync(
			string name,
			Condition? condition = null,
			bool? enableAutoscaling = null,
			int? minAgents = null,
			int? numReserveAgents = null,
			TimeSpan? conformInterval = null,
			TimeSpan? scaleOutCooldown = null,
			TimeSpan? scaleInCooldown = null,
			PoolSizeStrategy? sizeStrategy = null,
			LeaseUtilizationSettings? leaseUtilizationSettings = null,
			JobQueueSettings? jobQueueSettings = null,
			ComputeQueueAwsMetricSettings? computeQueueAwsMetricSettings = null,
			Dictionary<string, string>? properties = null)
		{
			return _pools.AddAsync(
				PoolId.Sanitize(name),
				name,
				condition,
				enableAutoscaling,
				minAgents,
				numReserveAgents,
				conformInterval,
				scaleOutCooldown,
				scaleInCooldown,
				sizeStrategy,
				leaseUtilizationSettings,
				jobQueueSettings,
				computeQueueAwsMetricSettings,
				properties);
		}

		/// <summary>
		/// Deletes a pool
		/// </summary>
		/// <param name="poolId">Unique id of the pool</param>
		/// <returns>Async task object</returns>
		public Task<bool> DeletePoolAsync(PoolId poolId)
		{
			return _pools.DeleteAsync(poolId);
		}

		/// <summary>
		/// Updates an existing pool
		/// </summary>
		/// <param name="pool">The pool to update</param>
		/// <param name="newName">The new name for the pool</param>
		/// <param name="newCondition">New requirements for the pool</param>
		/// <param name="newEnableAutoscaling">Whether to enable autoscaling</param>
		/// <param name="newMinAgents">Minimum number of agents in the pool</param>
		/// <param name="newNumReserveAgents">Minimum number of idle agents to maintain</param>
		/// <param name="newProperties">Properties on the pool to update. Any properties with a value of null will be removed.</param>
		/// <param name="conformInterval">Interval between conforms. Set to zero to disable.</param>
		/// <param name="scaleOutCooldown">Cooldown time between scale-out events</param>
		/// <param name="scaleInCooldown">Cooldown time between scale-in events</param>
		/// <param name="sizeStrategy">New pool sizing strategy for the pool</param>
		/// <param name="leaseUtilizationSettings">Settings for lease utilization-based strategy</param>
		/// <param name="jobQueueSettings">Settings for job queue-based strategy</param>
		/// <param name="computeQueueAwsMetricSettings">Settings for compute queue AWS metric strategy</param>
		/// <param name="useDefaultStrategy">Whether to use the default strategy</param>
		/// <returns>Async task object</returns>
		public async Task<IPool?> UpdatePoolAsync(
			IPool? pool,
			string? newName = null,
			Condition? newCondition = null,
			bool? newEnableAutoscaling = null,
			int? newMinAgents = null,
			int? newNumReserveAgents = null,
			Dictionary<string, string?>? newProperties = null,
			TimeSpan? conformInterval = null,
			TimeSpan? scaleOutCooldown = null,
			TimeSpan? scaleInCooldown = null,
			PoolSizeStrategy? sizeStrategy = null,
			LeaseUtilizationSettings? leaseUtilizationSettings = null,
			JobQueueSettings? jobQueueSettings = null,
			ComputeQueueAwsMetricSettings? computeQueueAwsMetricSettings = null,
			bool? useDefaultStrategy = null)
		{
			for (; pool != null; pool = await _pools.GetAsync(pool.Id))
			{
				IPool? newPool = await _pools.TryUpdateAsync(
					pool,
					newName,
					newCondition,
					newEnableAutoscaling,
					newMinAgents,
					newNumReserveAgents,
					newProperties: newProperties,
					conformInterval: conformInterval,
					scaleOutCooldown: scaleOutCooldown,
					scaleInCooldown: scaleInCooldown,
					sizeStrategy: sizeStrategy,
					leaseUtilizationSettings: leaseUtilizationSettings,
					jobQueueSettings: jobQueueSettings,
					computeQueueAwsMetricSettings: computeQueueAwsMetricSettings,
					useDefaultStrategy: useDefaultStrategy);
				
				if (newPool != null)
				{
					return newPool;
				}
			}
			return pool;
		}

		/// <summary>
		/// Gets all the available pools
		/// </summary>
		/// <returns>List of pool documents</returns>
		public Task<List<IPool>> GetPoolsAsync()
		{
			return _pools.GetAsync();
		}

		/// <summary>
		/// Gets a pool by ID
		/// </summary>
		/// <param name="poolId">Unique id of the pool</param>
		/// <returns>The pool document</returns>
		public Task<IPool?> GetPoolAsync(PoolId poolId)
		{
			return _pools.GetAsync(poolId);
		}

		/// <summary>
		/// Gets a cached pool definition
		/// </summary>
		/// <param name="poolId"></param>
		/// <param name="validAtTime"></param>
		/// <returns></returns>
		public async Task<IPool?> GetCachedPoolAsync(PoolId poolId, DateTime validAtTime)
		{
			Dictionary<PoolId, IPool> poolMapping = await GetPoolLookupAsync(validAtTime);
			poolMapping.TryGetValue(poolId, out IPool? pool);
			return pool;
		}

		/// <summary>
		/// Gets a cached pool definition
		/// </summary>
		/// <param name="agent"></param>
		/// <param name="validAtTime"></param>
		/// <returns></returns>
		public async Task<List<IPool>> GetCachedPoolsAsync(IAgent agent, DateTime validAtTime)
		{
			Dictionary<PoolId, IPool> poolMapping = await GetPoolLookupAsync(validAtTime);

			List<IPool> pools = new List<IPool>();
			foreach(PoolId poolId in agent.GetPools())
			{
				if(poolMapping.TryGetValue(poolId, out IPool? pool))
				{
					pools.Add(pool);
				}
			}

			return pools;
		}

		/// <summary>
		/// Get a list of workspaces for the given agent
		/// </summary>
		/// <param name="agent">The agent to return workspaces for</param>
		/// <param name="validAtTime">Absolute time at which we expect the results to be valid. Values may be cached as long as they are after this time.</param>
		/// <returns>List of workspaces</returns>
		public async Task<HashSet<AgentWorkspace>> GetWorkspacesAsync(IAgent agent, DateTime validAtTime)
		{
			bool bAddAutoSdkWorkspace = false;
			HashSet<AgentWorkspace> workspaces = new HashSet<AgentWorkspace>();

			Dictionary<PoolId, IPool> poolMapping = await GetPoolLookupAsync(validAtTime);
			foreach (PoolId poolId in agent.GetPools())
			{
				IPool? pool;
				if (poolMapping.TryGetValue(poolId, out pool))
				{
					workspaces.UnionWith(pool.Workspaces);
					bAddAutoSdkWorkspace |= pool.UseAutoSdk;
				}
			}

			if (bAddAutoSdkWorkspace)
			{
				Globals globals = await _mongoService.GetGlobalsAsync();
				workspaces.UnionWith(agent.GetAutoSdkWorkspaces(globals, workspaces.ToList()));
			}

			return workspaces;
		}

		/// <summary>
		/// Gets a mapping from pool identifiers to definitions
		/// </summary>
		/// <param name="validAtTime">Absolute time at which we expect the results to be valid. Values may be cached as long as they are after this time.</param>
		/// <returns>Map of pool ids to pool documents</returns>
		private async Task<Dictionary<PoolId, IPool>> GetPoolLookupAsync(DateTime validAtTime)
		{
			Tuple<DateTime, Dictionary<PoolId, IPool>>? cachedPoolLookupCopy = _cachedPoolLookup;
			if (cachedPoolLookupCopy == null || cachedPoolLookupCopy.Item1 < validAtTime)
			{
				// Get a new list of cached pools
				DateTime newCacheTime = _clock.UtcNow;
				List<IPool> newPools = await _pools.GetAsync();
				Tuple<DateTime, Dictionary<PoolId, IPool>> newCachedPoolLookup = Tuple.Create(newCacheTime, newPools.ToDictionary(x => x.Id, x => x));

				// Try to swap it with the current version
				while (cachedPoolLookupCopy == null || cachedPoolLookupCopy.Item1 < newCacheTime)
				{
					Tuple<DateTime, Dictionary<PoolId, IPool>>? originalValue = Interlocked.CompareExchange(ref _cachedPoolLookup, newCachedPoolLookup, cachedPoolLookupCopy);
					if (originalValue == cachedPoolLookupCopy)
					{
						cachedPoolLookupCopy = newCachedPoolLookup;
						break;
					}
					cachedPoolLookupCopy = originalValue;
				}
			}
			return cachedPoolLookupCopy.Item2;
		}
	}
}
