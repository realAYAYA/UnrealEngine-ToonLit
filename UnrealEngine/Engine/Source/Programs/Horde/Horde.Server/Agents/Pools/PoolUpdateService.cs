// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Agents.Pools;
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
	public sealed class PoolUpdateService : IHostedService, IAsyncDisposable
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
		public async ValueTask DisposeAsync()
		{
			await _updatePoolsTicker.DisposeAsync();
			await _shutdownDisabledAgentsTicker.DisposeAsync();
		}

		/// <summary>
		/// Shutdown agents that have been disabled for longer than the configured grace period
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the async task</param>
		/// <returns>Async task</returns>
		internal async ValueTask ShutdownDisabledAgentsAsync(CancellationToken cancellationToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(PoolUpdateService)}.{nameof(ShutdownDisabledAgentsAsync)}");

			IReadOnlyList<IPoolConfig> pools = await _pools.GetConfigsAsync(cancellationToken);
			IEnumerable<IAgent> disabledAgents = await _agents.FindAsync(enabled: false, cancellationToken: cancellationToken);
			disabledAgents = disabledAgents.Where(x => IsAgentAutoScaled(x, pools));

			int c = 0;
			foreach (IAgent agent in disabledAgents)
			{
				if (HasGracePeriodExpired(agent, pools, _globalConfig.CurrentValue.AgentShutdownIfDisabledGracePeriod))
				{
					await _agents.TryUpdateSettingsAsync(agent, requestShutdown: true, cancellationToken: cancellationToken);
					_logger.LogInformation("Shutting down agent {AgentId} as it has been disabled for longer than grace period", agent.Id.ToString());
					c++;
				}
			}

			span.SetAttribute("numShutdown", c);
		}

		private bool HasGracePeriodExpired(IAgent agent, IReadOnlyList<IPoolConfig> pools, TimeSpan globalGracePeriod)
		{
			if (agent.LastStatusChange == null)
			{
				return false;
			}

			TimeSpan gracePeriod = GetGracePeriod(agent, pools) ?? globalGracePeriod;
			DateTime expirationTime = agent.LastStatusChange.Value + gracePeriod;
			return _clock.UtcNow > expirationTime;
		}

		private static TimeSpan? GetGracePeriod(IAgent agent, IReadOnlyList<IPoolConfig> pools)
		{
			IEnumerable<PoolId> poolIds = agent.ExplicitPools.Concat(agent.DynamicPools);
			IPoolConfig? pool = pools.FirstOrDefault(x => poolIds.Contains(x.Id) && x.ShutdownIfDisabledGracePeriod != null);
			return pool?.ShutdownIfDisabledGracePeriod;
		}

		private static bool IsAgentAutoScaled(IAgent agent, IReadOnlyList<IPoolConfig> pools)
		{
			IEnumerable<PoolId> poolIds = agent.ExplicitPools.Concat(agent.DynamicPools);
			return pools.Any(x => poolIds.Contains(x.Id) && x.EnableAutoscaling);
		}

		/// <summary>
		/// Execute the background task
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the async task</param>
		/// <returns>Async task</returns>
		async ValueTask UpdatePoolsAsync(CancellationToken cancellationToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(PoolUpdateService)}.{nameof(UpdatePoolsAsync)}");

			// Capture the start time for this operation. We use this to attempt to sequence updates to agents, and prevent overriding another server's updates.
			DateTime startTime = DateTime.UtcNow;

			// Update the list
			bool retryUpdate = true;
			while (retryUpdate && !cancellationToken.IsCancellationRequested)
			{
				_logger.LogDebug("Updating pool->workspace map");

				// Assume this will be the last iteration
				retryUpdate = false;

				// Capture the list of pools at the start of this update
				IReadOnlyList<IPoolConfig> currentPools = await _pools.GetConfigsAsync(cancellationToken);

				// Lookup table of pool id to workspaces
				Dictionary<PoolId, AutoSdkConfig> poolToAutoSdkView = new Dictionary<PoolId, AutoSdkConfig>();
				Dictionary<PoolId, List<AgentWorkspaceInfo>> poolToAgentWorkspaces = new Dictionary<PoolId, List<AgentWorkspaceInfo>>();

				// Capture the current config state
				GlobalConfig globalConfig = _globalConfig.CurrentValue;

				// Populate the workspace list from the current stream
				foreach (StreamConfig streamConfig in globalConfig.Streams)
				{
					foreach (KeyValuePair<string, AgentConfig> agentTypePair in streamConfig.AgentTypes)
					{
						// Create the new agent workspace
						if (streamConfig.TryGetAgentWorkspace(agentTypePair.Value, out AgentWorkspaceInfo? agentWorkspace, out AutoSdkConfig? autoSdkConfig))
						{
							AgentConfig agentType = agentTypePair.Value;

							// Find or add a list of workspaces for this pool
							List<AgentWorkspaceInfo>? agentWorkspaces;
							if (!poolToAgentWorkspaces.TryGetValue(agentType.Pool, out agentWorkspaces))
							{
								agentWorkspaces = new List<AgentWorkspaceInfo>();
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
				foreach (IPoolConfig currentPool in currentPools)
				{
					// Get the new list of workspaces for this pool
					List<AgentWorkspaceInfo>? newWorkspaces;
					if (!poolToAgentWorkspaces.TryGetValue(currentPool.Id, out newWorkspaces))
					{
						newWorkspaces = new List<AgentWorkspaceInfo>();
					}

					// Get the autosdk view
					AutoSdkConfig? newAutoSdkConfig;
					if (!poolToAutoSdkView.TryGetValue(currentPool.Id, out newAutoSdkConfig))
					{
						newAutoSdkConfig = AutoSdkConfig.None;
					}

					// Update the pools document
					if (!AgentWorkspaceInfo.SetEquals(currentPool.Workspaces, newWorkspaces) || currentPool.Workspaces.Count != newWorkspaces.Count || !AutoSdkConfig.Equals(currentPool.AutoSdkConfig, newAutoSdkConfig))
					{
						_logger.LogInformation("New workspaces for pool {Pool}:{Workspaces}", currentPool.Id, String.Join("", newWorkspaces.Select(x => $"\n  Identifier=\"{x.Identifier}\", Stream={x.Stream}")));

						if (newAutoSdkConfig != null)
						{
							_logger.LogInformation("New autosdk view for pool {Pool}:{View}", currentPool.Id, String.Join("", newAutoSdkConfig.View.Select(x => $"\n  {x}")));
						}

#pragma warning disable CS0618 // Type or member is obsolete
						await _pools.UpdateConfigAsync(currentPool.Id, new UpdatePoolConfigOptions { Workspaces = newWorkspaces, AutoSdkConfig = newAutoSdkConfig }, cancellationToken);
#pragma warning restore CS0618 // Type or member is obsolete
					}
				}
			}
		}
	}
}
