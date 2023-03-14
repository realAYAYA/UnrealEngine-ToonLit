// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Horde.Build.Streams;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace Horde.Build.Agents.Pools
{
	using PoolId = StringId<IPool>;

	/// <summary>
	/// Periodically updates pool documents to contain the correct workspaces
	/// </summary>
	public sealed class PoolUpdateService : IHostedService, IDisposable
	{
		readonly IPoolCollection _pools;
		readonly IStreamCollection _streams;
		readonly ILogger<PoolService> _logger;
		readonly ITicker _ticker;

		/// <summary>
		/// Constructor
		/// </summary>
		public PoolUpdateService(IPoolCollection pools, IStreamCollection streams, IClock clock, ILogger<PoolService> logger)
		{
			_pools = pools;
			_streams = streams;
			_logger = logger;
			_ticker = clock.AddTicker($"{nameof(PoolUpdateService)}.{nameof(UpdatePoolsAsync)}", TimeSpan.FromSeconds(30.0), UpdatePoolsAsync, logger);
		}

		/// <inheritdoc/>
		public Task StartAsync(CancellationToken cancellationToken) => _ticker.StartAsync();

		/// <inheritdoc/>
		public Task StopAsync(CancellationToken cancellationToken) => _ticker.StopAsync();

		/// <inheritdoc/>
		public void Dispose() => _ticker.Dispose();

		/// <summary>
		/// Execute the background task
		/// </summary>
		/// <param name="stoppingToken">Cancellation token for the async task</param>
		/// <returns>Async task</returns>
		async ValueTask UpdatePoolsAsync(CancellationToken stoppingToken)
		{
			// Capture the start time for this operation. We use this to attempt to sequence updates to agents, and prevent overriding another server's updates.
			DateTime startTime = DateTime.UtcNow;

			// Update the list
			bool bRetryUpdate = true;
			while (bRetryUpdate && !stoppingToken.IsCancellationRequested)
			{
				_logger.LogDebug("Updating pool->workspace map");

				// Assume this will be the last iteration
				bRetryUpdate = false;

				// Capture the list of pools at the start of this update
				List<IPool> currentPools = await _pools.GetAsync();

				// Lookup table of pool id to workspaces
				HashSet<PoolId> poolsWithAutoSdk = new HashSet<PoolId>();
				Dictionary<PoolId, List<AgentWorkspace>> poolToAgentWorkspaces = new Dictionary<PoolId, List<AgentWorkspace>>();

				// Populate the workspace list from the current stream
				List<IStream> activeStreams = await _streams.FindAllAsync();
				foreach (IStream activeStream in activeStreams)
				{
					foreach (KeyValuePair<string, AgentType> agentTypePair in activeStream.AgentTypes)
					{
						// Create the new agent workspace
						(AgentWorkspace, bool)? result;
						if (activeStream.TryGetAgentWorkspace(agentTypePair.Value, out result))
						{
							(AgentWorkspace agentWorkspace, bool useAutoSdk) = result.Value;
							AgentType agentType = agentTypePair.Value;

							// Find or add a list of workspaces for this pool
							List<AgentWorkspace>? agentWorkspaces;
							if (!poolToAgentWorkspaces.TryGetValue(agentType.Pool, out agentWorkspaces))
							{
								agentWorkspaces = new List<AgentWorkspace>();
								poolToAgentWorkspaces.Add(agentType.Pool, agentWorkspaces);
							}

							// Add it to the list
							if (!agentWorkspaces.Contains(agentWorkspace))
							{
								agentWorkspaces.Add(agentWorkspace);
							}
							if (useAutoSdk)
							{
								poolsWithAutoSdk.Add(agentType.Pool);
							}
						}
					}
				}

				// Update the list of workspaces for each pool
				foreach (IPool currentPool in currentPools)
				{
					// Get the new list of workspaces for this pool
					List<AgentWorkspace>? newWorkspaces;
					if (!poolToAgentWorkspaces.TryGetValue(currentPool.Id, out newWorkspaces))
					{
						newWorkspaces = new List<AgentWorkspace>();
					}

					// Update the pools document
					bool useAutoSdk = poolsWithAutoSdk.Contains(currentPool.Id);
					if (!AgentWorkspace.SetEquals(currentPool.Workspaces, newWorkspaces) || currentPool.Workspaces.Count != newWorkspaces.Count || currentPool.UseAutoSdk != useAutoSdk)
					{
						_logger.LogInformation("New workspaces for pool {Pool}:{Workspaces}", currentPool.Id, String.Join("", newWorkspaces.Select(x => $"\n  Identifier=\"{x.Identifier}\", Stream={x.Stream}")));

						IPool? result = await _pools.TryUpdateAsync(currentPool, newWorkspaces: newWorkspaces, newUseAutoSdk: useAutoSdk);
						if (result == null)
						{
							_logger.LogInformation("Pool modified; will retry");
							bRetryUpdate = true;
						}
					}
				}
			}
		}
	}
}
