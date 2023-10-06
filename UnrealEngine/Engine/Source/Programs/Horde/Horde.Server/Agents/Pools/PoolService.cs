// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde;
using EpicGames.Horde.Common;
using Horde.Server.Agents.Fleet;
using Horde.Server.Server;
using Horde.Server.Utilities;
using HordeCommon;

namespace Horde.Server.Agents.Pools
{
	/// <summary>
	/// Wraps functionality for manipulating pools
	/// </summary>
	public class PoolService
	{
		/// <summary>
		/// The globals service instance
		/// </summary>
		readonly GlobalsService _globalsService;

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
		/// <param name="globalsService"></param>
		/// <param name="pools">Collection of pool documents</param>
		/// <param name="clock"></param>
		public PoolService(GlobalsService globalsService, IPoolCollection pools, IClock clock)
		{
			_globalsService = globalsService;
			_pools = pools;
			_clock = clock;
		}

		/// <summary>
		/// Creates a new pool
		/// </summary>
		/// <param name="name">Name of the new pool</param>
		/// <param name="options">Options for the new pool</param>
		/// <returns>The new pool document</returns>
		public Task<IPool> CreatePoolAsync(string name, AddPoolOptions options)
		{
			return _pools.AddAsync(new PoolId(StringId.Sanitize(name)), name, options);
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
		/// <param name="options">Options for the update</param>
		/// <returns>Async task object</returns>
		public async Task<IPool?> UpdatePoolAsync(IPool? pool, UpdatePoolOptions options)
		{
			for (; pool != null; pool = await _pools.GetAsync(pool.Id))
			{
				IPool? newPool = await _pools.TryUpdateAsync(pool, options);
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
		/// <param name="globalConfig">Current configuration</param>
		/// <returns>List of workspaces</returns>
		public async Task<HashSet<AgentWorkspace>> GetWorkspacesAsync(IAgent agent, DateTime validAtTime, GlobalConfig globalConfig)
		{
			AutoSdkConfig? autoSdkConfig = null;
			HashSet<AgentWorkspace> workspaces = new HashSet<AgentWorkspace>();

			Dictionary<PoolId, IPool> poolMapping = await GetPoolLookupAsync(validAtTime);
			foreach (PoolId poolId in agent.GetPools())
			{
				IPool? pool;
				if (poolMapping.TryGetValue(poolId, out pool))
				{
					workspaces.UnionWith(pool.Workspaces);
					autoSdkConfig = AutoSdkConfig.Merge(autoSdkConfig, pool.AutoSdkConfig);
				}
			}

			if (autoSdkConfig != null)
			{
				workspaces.UnionWith(agent.GetAutoSdkWorkspaces(globalConfig, autoSdkConfig, workspaces.ToList()));
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
