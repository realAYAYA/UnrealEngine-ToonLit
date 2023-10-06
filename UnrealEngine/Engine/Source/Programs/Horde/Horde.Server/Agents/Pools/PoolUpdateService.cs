// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Horde.Server.Server;
using Horde.Server.Streams;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using OpenTelemetry.Trace;

namespace Horde.Server.Agents.Pools
{
	/// <summary>
	/// Periodically updates pool documents to contain the correct workspaces
	/// </summary>
	public sealed class PoolUpdateService : IHostedService, IDisposable
	{
		readonly IAgentCollection _agents;
		readonly IPoolCollection _pools;
		readonly IClock _clock;
		readonly IOptionsMonitor<GlobalConfig> _globalConfig;
		readonly ILogger<PoolUpdateService> _logger;
		readonly Tracer _tracer;
		readonly ITicker _updatePoolsTicker;
		readonly ITicker _shutdownDisabledAgentsTicker;

		/// <summary>
		/// Constructor
		/// </summary>
		public PoolUpdateService(IAgentCollection agents, IPoolCollection pools, IClock clock, IOptionsMonitor<GlobalConfig> globalConfig, Tracer tracer, ILogger<PoolUpdateService> logger)
		{
			_agents = agents;
			_pools = pools;
			_clock = clock;
			_globalConfig = globalConfig;
			_tracer = tracer;
			_logger = logger;
			_updatePoolsTicker = clock.AddTicker($"{nameof(PoolUpdateService)}.{nameof(UpdatePoolsAsync)}", TimeSpan.FromSeconds(30.0), UpdatePoolsAsync, logger);
			_shutdownDisabledAgentsTicker = clock.AddSharedTicker($"{nameof(PoolUpdateService)}.{nameof(ShutdownDisabledAgentsAsync)}", TimeSpan.FromHours(1), ShutdownDisabledAgentsAsync, logger);
		}

		/// <inheritdoc/>
		public async Task StartAsync(CancellationToken cancellationToken)
		{
			await _updatePoolsTicker.StartAsync();
			await _shutdownDisabledAgentsTicker.StartAsync();
		}

		/// <inheritdoc/>
		public async Task StopAsync(CancellationToken cancellationToken)
		{
			await _updatePoolsTicker.StopAsync();
			await _shutdownDisabledAgentsTicker.StopAsync();
		} 

		/// <inheritdoc/>
		public void Dispose()
		{
			_updatePoolsTicker.Dispose();
			_shutdownDisabledAgentsTicker.Dispose();
		}

		/// <summary>
		/// Shutdown agents that have been disabled for longer than the configured grace period
		/// </summary>
		/// <param name="stoppingToken">Cancellation token for the async task</param>
		/// <returns>Async task</returns>
		internal async ValueTask ShutdownDisabledAgentsAsync(CancellationToken stoppingToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(PoolUpdateService)}.{nameof(ShutdownDisabledAgentsAsync)}");

			List<IPool> pools = await _pools.GetAsync();
			IEnumerable<IAgent> disabledAgents = await _agents.FindAsync(enabled: false);
			disabledAgents = disabledAgents.Where(x => IsAgentAutoScaled(x, pools));

			int c = 0;
			foreach (IAgent agent in disabledAgents)
			{
				if (HasGracePeriodExpired(agent, pools, _globalConfig.CurrentValue.AgentShutdownIfDisabledGracePeriod))
				{
					await _agents.TryUpdateSettingsAsync(agent, requestShutdown: true);
					_logger.LogInformation("Shutting down agent {AgentId} as it has been disabled for longer than grace period", agent.Id.ToString());
					c++;
				}
			}

			span.SetAttribute("numShutdown", c);
		}
		
		private bool HasGracePeriodExpired(IAgent agent, List<IPool> pools, TimeSpan globalGracePeriod)
		{
			if (agent.LastStatusChange == null)
			{
				return false;
			}

			TimeSpan gracePeriod = GetGracePeriod(agent, pools) ?? globalGracePeriod;
			DateTime expirationTime = agent.LastStatusChange.Value + gracePeriod;
			return _clock.UtcNow > expirationTime;
		}

		private static TimeSpan? GetGracePeriod(IAgent agent, List<IPool> pools)
		{
			IEnumerable<PoolId> poolIds = agent.ExplicitPools.Concat(agent.DynamicPools);
			IPool? pool = pools.FirstOrDefault(x => poolIds.Contains(x.Id) && x.ShutdownIfDisabledGracePeriod != null);
			return pool?.ShutdownIfDisabledGracePeriod;
		}
		
		private static bool IsAgentAutoScaled(IAgent agent, List<IPool> pools)
		{
			IEnumerable<PoolId> poolIds = agent.ExplicitPools.Concat(agent.DynamicPools);
			return pools.Find(x => poolIds.Contains(x.Id) && x.EnableAutoscaling) != null;
		}
		
		/// <summary>
		/// Execute the background task
		/// </summary>
		/// <param name="stoppingToken">Cancellation token for the async task</param>
		/// <returns>Async task</returns>
		async ValueTask UpdatePoolsAsync(CancellationToken stoppingToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(PoolUpdateService)}.{nameof(UpdatePoolsAsync)}");
			
			// Capture the start time for this operation. We use this to attempt to sequence updates to agents, and prevent overriding another server's updates.
			DateTime startTime = DateTime.UtcNow;

			// Update the list
			bool retryUpdate = true;
			while (retryUpdate && !stoppingToken.IsCancellationRequested)
			{
				_logger.LogDebug("Updating pool->workspace map");

				// Assume this will be the last iteration
				retryUpdate = false;

				// Capture the list of pools at the start of this update
				List<IPool> currentPools = await _pools.GetAsync();

				// Lookup table of pool id to workspaces
				Dictionary<PoolId, AutoSdkConfig> poolToAutoSdkView = new Dictionary<PoolId, AutoSdkConfig>();
				Dictionary<PoolId, List<AgentWorkspace>> poolToAgentWorkspaces = new Dictionary<PoolId, List<AgentWorkspace>>();

				// Capture the current config state
				GlobalConfig globalConfig = _globalConfig.CurrentValue;

				// Populate the workspace list from the current stream
				foreach (StreamConfig streamConfig in globalConfig.Streams)
				{
					foreach (KeyValuePair<string, AgentConfig> agentTypePair in streamConfig.AgentTypes)
					{
						// Create the new agent workspace
						if (streamConfig.TryGetAgentWorkspace(agentTypePair.Value, out AgentWorkspace? agentWorkspace, out AutoSdkConfig? autoSdkConfig))
						{
							AgentConfig agentType = agentTypePair.Value;

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
							if (autoSdkConfig != null)
							{
								AutoSdkConfig? existingAutoSdkConfig;
								poolToAutoSdkView.TryGetValue(agentType.Pool, out existingAutoSdkConfig);
								poolToAutoSdkView[agentType.Pool] = AutoSdkConfig.Merge(autoSdkConfig, existingAutoSdkConfig);
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

					// Get the autosdk view
					AutoSdkConfig? newAutoSdkConfig;
					if (!poolToAutoSdkView.TryGetValue(currentPool.Id, out newAutoSdkConfig))
					{
						newAutoSdkConfig = AutoSdkConfig.None;
					}

					// Update the pools document
					if (!AgentWorkspace.SetEquals(currentPool.Workspaces, newWorkspaces) || currentPool.Workspaces.Count != newWorkspaces.Count || !AutoSdkConfig.Equals(currentPool.AutoSdkConfig, newAutoSdkConfig))
					{
						_logger.LogInformation("New workspaces for pool {Pool}:{Workspaces}", currentPool.Id, String.Join("", newWorkspaces.Select(x => $"\n  Identifier=\"{x.Identifier}\", Stream={x.Stream}")));

						if (newAutoSdkConfig != null)
						{
							_logger.LogInformation("New autosdk view for pool {Pool}:{View}", currentPool.Id, String.Join("", newAutoSdkConfig.View.Select(x => $"\n  {x}"))); 
						}

						IPool? result = await _pools.TryUpdateAsync(currentPool, new UpdatePoolOptions { Workspaces = newWorkspaces, AutoSdkConfig = newAutoSdkConfig });
						if (result == null)
						{
							_logger.LogInformation("Pool modified; will retry");
							retryUpdate = true;
						}
					}
				}
			}
		}
	}
}
