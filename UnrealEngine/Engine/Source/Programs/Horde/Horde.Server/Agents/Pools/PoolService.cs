// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde;
using EpicGames.Horde.Agents.Pools;
using Horde.Server.Server;
using HordeCommon;

namespace Horde.Server.Agents.Pools
{
	/// <summary>
	/// Wraps functionality for manipulating pools
	/// </summary>
	public class PoolService
	{
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
		Tuple<DateTime, Dictionary<PoolId, IPoolConfig>>? _cachedPoolLookup;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="pools">Collection of pool documents</param>
		/// <param name="clock"></param>
		public PoolService(IPoolCollection pools, IClock clock)
		{
			_pools = pools;
			_clock = clock;
		}

		/// <summary>
		/// Creates a new pool
		/// </summary>
		/// <param name="name">Name of the new pool</param>
		/// <param name="options">Options for the new pool</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The new pool document</returns>
		[Obsolete("Pools should be configured through globals.json")]
		public Task CreatePoolAsync(string name, CreatePoolConfigOptions options, CancellationToken cancellationToken = default)
		{
			return _pools.CreateConfigAsync(new PoolId(StringId.Sanitize(name)), name, options, cancellationToken);
		}

		/// <summary>
		/// Deletes a pool
		/// </summary>
		/// <param name="poolId">Unique id of the pool</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Async task object</returns>
		[Obsolete("Pools should be configured through globals.json")]
		public Task<bool> DeletePoolAsync(PoolId poolId, CancellationToken cancellationToken = default)
		{
			return _pools.DeleteConfigAsync(poolId, cancellationToken);
		}

		/// <summary>
		/// Updates an existing pool
		/// </summary>
		/// <param name="poolId">The pool to update</param>
		/// <param name="options">Options for the update</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Async task object</returns>
		[Obsolete("Pools should be configured through globals.json")]
		public Task UpdateConfigAsync(PoolId poolId, UpdatePoolConfigOptions options, CancellationToken cancellationToken)
		{
			return _pools.UpdateConfigAsync(poolId, options, cancellationToken);
		}

		/// <summary>
		/// Gets all the available pools
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of pool documents</returns>
		public Task<IReadOnlyList<IPoolConfig>> GetPoolsAsync(CancellationToken cancellationToken)
		{
			return _pools.GetConfigsAsync(cancellationToken);
		}

		/// <summary>
		/// Gets a pool by ID
		/// </summary>
		/// <param name="poolId">Unique id of the pool</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The pool document</returns>
		public Task<IPool?> GetPoolAsync(PoolId poolId, CancellationToken cancellationToken = default)
		{
			return _pools.GetAsync(poolId, cancellationToken);
		}

		/// <summary>
		/// Gets a cached pool definition
		/// </summary>
		/// <param name="poolId"></param>
		/// <param name="validAtTime"></param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public async Task<IPoolConfig?> GetPoolAsync(PoolId poolId, DateTime validAtTime, CancellationToken cancellationToken)
		{
			Dictionary<PoolId, IPoolConfig> poolMapping = await GetPoolLookupAsync(validAtTime, cancellationToken);
			poolMapping.TryGetValue(poolId, out IPoolConfig? pool);
			return pool;
		}

		/// <summary>
		/// Gets a cached pool definition
		/// </summary>
		/// <param name="agent"></param>
		/// <param name="validAtTime"></param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public async Task<List<IPoolConfig>> GetPoolsAsync(IAgent agent, DateTime validAtTime, CancellationToken cancellationToken)
		{
			Dictionary<PoolId, IPoolConfig> poolMapping = await GetPoolLookupAsync(validAtTime, cancellationToken);

			List<IPoolConfig> pools = new List<IPoolConfig>();
			foreach (PoolId poolId in agent.GetPools())
			{
				if (poolMapping.TryGetValue(poolId, out IPoolConfig? pool))
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
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of workspaces</returns>
		public async Task<HashSet<AgentWorkspaceInfo>> GetWorkspacesAsync(IAgent agent, DateTime validAtTime, GlobalConfig globalConfig, CancellationToken cancellationToken)
		{
			List<IPoolConfig> pools = await GetPoolsAsync(agent, validAtTime, cancellationToken);

			HashSet<AgentWorkspaceInfo> workspaces = new HashSet<AgentWorkspaceInfo>();
			foreach (IPoolConfig pool in pools)
			{
				workspaces.UnionWith(pool.Workspaces);
			}

			AutoSdkConfig? autoSdkConfig = GetAutoSdkConfig(pools);
			if (autoSdkConfig != null)
			{
				foreach (string? clusterName in workspaces.Select(x => x.Cluster).Distinct().ToList())
				{
					PerforceCluster? cluster = globalConfig.FindPerforceCluster(clusterName);
					if (cluster != null)
					{
						AgentWorkspaceInfo? autoSdkWorkspace = agent.GetAutoSdkWorkspace(cluster, autoSdkConfig);
						if (autoSdkWorkspace != null)
						{
							workspaces.Add(autoSdkWorkspace);
						}
					}
				}
			}

			return workspaces;
		}

		static AutoSdkConfig? GetAutoSdkConfig(IEnumerable<IPoolConfig> pools)
		{
			AutoSdkConfig? autoSdkConfig = null;
			foreach (IPoolConfig pool in pools)
			{
				autoSdkConfig = AutoSdkConfig.Merge(autoSdkConfig, pool.AutoSdkConfig);
			}
			return autoSdkConfig;
		}

		/// <summary>
		/// Gets all the autosdk workspaces required for an agent
		/// </summary>
		/// <param name="agent"></param>
		/// <param name="cluster"></param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public async Task<AgentWorkspaceInfo?> GetAutoSdkWorkspaceAsync(IAgent agent, PerforceCluster cluster, CancellationToken cancellationToken)
		{
			List<IPoolConfig> pools = await GetPoolsAsync(agent, DateTime.UtcNow - TimeSpan.FromSeconds(10.0), cancellationToken);

			AutoSdkConfig? autoSdkConfig = GetAutoSdkConfig(pools);
			if (autoSdkConfig == null)
			{
				return null;
			}

			return agent.GetAutoSdkWorkspace(cluster, autoSdkConfig);
		}

		/// <summary>
		/// Get a list of workspaces for the given agent
		/// </summary>
		/// <param name="agent">The agent to return workspaces for</param>
		/// <param name="perforceCluster">The P4 cluster to find a workspace for</param>
		/// <param name="validAtTime">Absolute time at which we expect the results to be valid. Values may be cached as long as they are after this time.</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of workspaces</returns>
		public async Task<AgentWorkspaceInfo?> GetAutoSdkWorkspaceAsync(IAgent agent, PerforceCluster perforceCluster, DateTime validAtTime, CancellationToken cancellationToken)
		{
			AutoSdkConfig? autoSdkConfig = null;

			Dictionary<PoolId, IPoolConfig> poolMapping = await GetPoolLookupAsync(validAtTime, cancellationToken);
			foreach (PoolId poolId in agent.GetPools())
			{
				IPoolConfig? pool;
				if (poolMapping.TryGetValue(poolId, out pool))
				{
					autoSdkConfig = AutoSdkConfig.Merge(autoSdkConfig, pool.AutoSdkConfig);
				}
			}

			if (autoSdkConfig == null)
			{
				return null;
			}
			else
			{
				return agent.GetAutoSdkWorkspace(perforceCluster, autoSdkConfig);
			}
		}

		/// <summary>
		/// Gets a mapping from pool identifiers to definitions
		/// </summary>
		/// <param name="validAtTime">Absolute time at which we expect the results to be valid. Values may be cached as long as they are after this time.</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Map of pool ids to pool documents</returns>
		private async Task<Dictionary<PoolId, IPoolConfig>> GetPoolLookupAsync(DateTime validAtTime, CancellationToken cancellationToken)
		{
			Tuple<DateTime, Dictionary<PoolId, IPoolConfig>>? cachedPoolLookupCopy = _cachedPoolLookup;
			if (cachedPoolLookupCopy == null || cachedPoolLookupCopy.Item1 < validAtTime)
			{
				// Get a new list of cached pools
				DateTime newCacheTime = _clock.UtcNow;
				IReadOnlyList<IPoolConfig> newPools = await _pools.GetConfigsAsync(cancellationToken);
				Tuple<DateTime, Dictionary<PoolId, IPoolConfig>> newCachedPoolLookup = Tuple.Create(newCacheTime, newPools.ToDictionary(x => x.Id, x => x));

				// Try to swap it with the current version
				while (cachedPoolLookupCopy == null || cachedPoolLookupCopy.Item1 < newCacheTime)
				{
					Tuple<DateTime, Dictionary<PoolId, IPoolConfig>>? originalValue = Interlocked.CompareExchange(ref _cachedPoolLookup, newCachedPoolLookup, cachedPoolLookupCopy);
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
