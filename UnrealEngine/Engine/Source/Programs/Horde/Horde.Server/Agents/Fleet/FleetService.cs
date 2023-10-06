// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.Metrics;
using System.Linq;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using Horde.Server.Agents.Leases;
using Horde.Server.Agents.Pools;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Server;
using Horde.Server.Streams;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using OpenTelemetry.Trace;

namespace Horde.Server.Agents.Fleet
{
	/// <summary>
	/// Parameters required for calculating pool size
	/// </summary>
	public class PoolWithAgents
	{
		/// <summary>
		/// Pool being resized
		/// </summary>
		public IPool Pool { get; }
		
		/// <summary>
		/// All agents currently associated with the pool
		/// </summary>
		public List<IAgent> Agents { get; }
	
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="pool"></param>
		/// <param name="agents"></param>
		public PoolWithAgents(IPool pool, List<IAgent> agents)
		{
			Pool = pool;
			Agents = agents;
		}
	}
	
	/// <summary>
	/// Service for managing the autoscaling of agent pools
	/// </summary>
	public sealed class FleetService : IHostedService, IDisposable
	{
		/// <summary>
		/// Max number of auto-scaling calculations to be done concurrently (sizing calculations and fleet manager calls)
		/// </summary>
		private const int MaxParallelTasks = 10;
		
		private readonly IAgentCollection _agentCollection;
		private readonly IGraphCollection _graphCollection;
		private readonly IJobCollection _jobCollection;
		private readonly ILeaseCollection _leaseCollection;
		private readonly IPoolCollection _poolCollection;
		private readonly IDowntimeService _downtimeService;
		private readonly IStreamCollection _streamCollection;
		private readonly Meter _meter;
		private readonly IFleetManagerFactory _fleetManagerFactory;
		private readonly IClock _clock;
		private readonly IMemoryCache _cache;
		private readonly ITicker _ticker;
		private readonly ITicker _tickerHighFrequency;
		private readonly TimeSpan _defaultScaleOutCooldown;
		private readonly TimeSpan _defaultScaleInCooldown;
		private readonly IOptions<ServerSettings> _settings;
		private readonly IOptionsMonitor<GlobalConfig> _globalConfig;
		private readonly Tracer _tracer;
		private readonly ILogger<FleetService> _logger;
		private readonly Gauge<int> _agentCountCurrentGauge;
		private readonly Gauge<int> _agentCountTargetGauge;
		
		/// <summary>
		/// Constructor
		/// </summary>
		public FleetService(
			IAgentCollection agentCollection,
			IGraphCollection graphCollection,
			IJobCollection jobCollection,
			ILeaseCollection leaseCollection,
			IPoolCollection poolCollection,
			IDowntimeService downtimeService,
			IStreamCollection streamCollection,
			Meter meter,
			IFleetManagerFactory fleetManagerFactory,
			IClock clock,
			IMemoryCache cache,
			IOptions<ServerSettings> settings,
			IOptionsMonitor<GlobalConfig> globalConfig,
			Tracer tracer,
			ILogger<FleetService> logger)
		{
			_agentCollection = agentCollection;
			_graphCollection = graphCollection;
			_jobCollection = jobCollection;
			_leaseCollection = leaseCollection;
			_poolCollection = poolCollection;
			_downtimeService = downtimeService;
			_streamCollection = streamCollection;
			_meter = meter;
			_fleetManagerFactory = fleetManagerFactory;
			_clock = clock;
			_cache = cache;
			_globalConfig = globalConfig;
			_tracer = tracer;
			_logger = logger;
			_ticker = clock.AddSharedTicker<FleetService>(TimeSpan.FromSeconds(30), TickLeaderAsync, _logger);
			_tickerHighFrequency = clock.AddSharedTicker("FleetService.TickHighFrequency", TimeSpan.FromSeconds(30), TickHighFrequencyAsync, _logger);
			_settings = settings;
			_defaultScaleOutCooldown = TimeSpan.FromSeconds(settings.Value.AgentPoolScaleOutCooldownSeconds);
			_defaultScaleInCooldown = TimeSpan.FromSeconds(settings.Value.AgentPoolScaleInCooldownSeconds);

			_agentCountCurrentGauge = _meter.CreateGauge<int>("horde.fleet.agentCountCurrent");
			_agentCountTargetGauge = _meter.CreateGauge<int>("horde.fleet.agentCountTarget");
		}

		/// <inheritdoc/>
		public Task StartAsync(CancellationToken cancellationToken)
		{
			return Task.WhenAll(_ticker.StartAsync(), _tickerHighFrequency.StartAsync());
		}

		/// <inheritdoc/>
		public Task StopAsync(CancellationToken cancellationToken)
		{
			return Task.WhenAll(_ticker.StopAsync(), _tickerHighFrequency.StopAsync());
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_ticker.Dispose();
			_tickerHighFrequency.Dispose();
		}

		internal async ValueTask TickLeaderAsync(CancellationToken stoppingToken)
		{
			TelemetrySpan span = _tracer.StartSpan($"{nameof(FleetService)}.{nameof(TickLeaderAsync)}");

			try
			{
				List<PoolWithAgents> poolsWithAgents = await GetPoolsWithAgentsAsync();
			
				ParallelOptions options = new () { MaxDegreeOfParallelism = MaxParallelTasks, CancellationToken = stoppingToken };
				await Parallel.ForEachAsync(poolsWithAgents, options, async (input, innerCt) =>
				{
					try
					{
						await CalculateSizeAndScaleAsync(input.Pool, input.Agents, innerCt);
					}
					catch (Exception e)
					{
						_logger.LogError(e, "Failed to scale pool {PoolId}", input.Pool.Id);
					}
				});
			}
			finally
			{
				span.End();
			}
		}

		internal async ValueTask TickHighFrequencyAsync(CancellationToken stoppingToken)
		{
			TelemetrySpan span = _tracer.StartSpan($"{nameof(FleetService)}.{nameof(TickHighFrequencyAsync)}");
			try
			{
				// TODO: Re-enable high frequency scaling (only used for experimental scaling of remote execution agents at the moment)
				await Task.Delay(0, stoppingToken);
			}
			finally
			{
				span.End();
			}
		}

		internal async Task CalculateSizeAndScaleAsync(IPool pool, List<IAgent> agents, CancellationToken cancellationToken)
		{
			IPoolSizeStrategy sizeStrategy = CreatePoolSizeStrategy(pool);
			PoolSizeResult result = await sizeStrategy.CalculatePoolSizeAsync(pool, agents);
			await ScalePoolAsync(pool, agents, result, cancellationToken);
		}

		internal async Task<List<PoolWithAgents>> GetPoolsWithAgentsAsync()
		{
			List<IAgent> agents = (await _agentCollection.FindAsync(status: AgentStatus.Ok, enabled: true)).Where(x => !x.RequestShutdown).ToList();
			List<IAgent> GetAgentsInPool(PoolId poolId) => agents.FindAll(a => a.GetPools().Any(p => p == poolId));
			List<IPool> pools = await _poolCollection.GetAsync();

			return pools.Select(pool => new PoolWithAgents(pool, GetAgentsInPool(pool.Id))).ToList();
		}

		internal async Task<ScaleResult> ScalePoolAsync(IPool pool, List<IAgent> agents, PoolSizeResult poolSizeResult, CancellationToken cancellationToken)
		{
			if (!pool.EnableAutoscaling)
			{
				return new ScaleResult(FleetManagerOutcome.NoOp, 0, 0, "Auto-scaling disabled");
			}

			int currentAgentCount = poolSizeResult.CurrentAgentCount;
			int desiredAgentCount = poolSizeResult.DesiredAgentCount;
			int deltaAgentCount = desiredAgentCount - currentAgentCount;

			IFleetManager fleetManager = CreateFleetManager(pool);
			
			using TelemetrySpan span = _tracer.StartSpan($"{nameof(FleetService)}.{nameof(ScalePoolAsync)}");
			span.SetAttribute(OpenTelemetryTracers.DatadogResourceAttribute, pool.Id.ToString());
			span.SetAttribute("currentAgentCount", currentAgentCount);
			span.SetAttribute("desiredAgentCount", desiredAgentCount);
			span.SetAttribute("deltaAgentCount", deltaAgentCount);
			span.SetAttribute("fleetManager", fleetManager.GetType().Name);

			Dictionary<string, object> logScopeMetadata = new()
			{
				["FleetManager"] = fleetManager.GetType().Name,
				["PoolSizeStrategy"] = poolSizeResult.Status ?? new Dictionary<string, object>(),
				["PoolId"] = pool.Id,
			};

			using IDisposable logScope = _logger.BeginScope(logScopeMetadata);
			if (pool.LastAgentCount != currentAgentCount || pool.LastDesiredAgentCount != desiredAgentCount)
			{
				_logger.LogInformation("{PoolName} Current={Current} Target={Target} Delta={Delta}",
					pool.Name, currentAgentCount, desiredAgentCount, deltaAgentCount);	
			}

			_agentCountCurrentGauge.Record(currentAgentCount, KeyValuePair.Create("pool", (object?)pool.Name));
			_agentCountTargetGauge.Record(desiredAgentCount, KeyValuePair.Create("pool", (object?)pool.Name));

			DateTime? scaleOutTime = null;
			DateTime? scaleInTime = null;
			
			TimeSpan scaleOutCooldown = pool.ScaleOutCooldown ?? _defaultScaleOutCooldown;
			bool isScaleOutCoolingDown = pool.LastScaleUpTime != null && pool.LastScaleUpTime + scaleOutCooldown > _clock.UtcNow;
			TimeSpan? scaleOutCooldownTimeLeft = pool.LastScaleUpTime + _defaultScaleOutCooldown - _clock.UtcNow;
			span.SetAttribute("scaleOutCooldownTimeLeftSecs", scaleOutCooldownTimeLeft?.TotalSeconds ?? -1);
			
			TimeSpan scaleInCooldown = pool.ScaleInCooldown ?? _defaultScaleInCooldown;
			bool isScaleInCoolingDown = pool.LastScaleDownTime != null && pool.LastScaleDownTime + scaleInCooldown > _clock.UtcNow;
			TimeSpan? scaleInCooldownTimeLeft = pool.LastScaleDownTime + _defaultScaleInCooldown - _clock.UtcNow;
			span.SetAttribute("scaleInCooldownTimeLeftSecs", scaleInCooldownTimeLeft?.TotalSeconds ?? -1);
			
			span.SetAttribute("isDowntimeActive", _downtimeService.IsDowntimeActive);
			
			ScaleResult cooldownActiveResult = new(FleetManagerOutcome.NoOp, 0, 0, "Cooldown active");
			ScaleResult result = new (FleetManagerOutcome.NoOp, 0, 0);
			try
			{
				if (deltaAgentCount > 0)
				{
					if (_downtimeService.IsDowntimeActive)
					{
						result = new (FleetManagerOutcome.NoOp, 0, 0, "Downtime is active");
					}
					else if (isScaleOutCoolingDown)
					{
						result = cooldownActiveResult;
					}
					else
					{
						result = await ExpandWithPendingShutdownsFirstAsync(pool, deltaAgentCount, (agentsToAdd) =>
							fleetManager.ExpandPoolAsync(pool, agents, agentsToAdd, cancellationToken));
						
						scaleOutTime = _clock.UtcNow;
					}
				}
				else if (deltaAgentCount < 0)
				{
					if (isScaleInCoolingDown)
					{
						result = cooldownActiveResult;
					}
					else
					{
						result = await fleetManager.ShrinkPoolAsync(pool, agents, -deltaAgentCount, cancellationToken);
						scaleInTime = _clock.UtcNow;
					}
				}
			}
			catch (Exception ex)
			{
				_logger.LogInformation(ex, "Failed to scale {PoolName}", pool.Name);
				return new ScaleResult(FleetManagerOutcome.Failure, 0, 0, "Exception during scaling. See log.");
			}

			bool isResultDifferentFromLastTime = pool.LastScaleResult == null || !pool.LastScaleResult.Equals(result);
			bool isCooldownResult = ReferenceEquals(result, cooldownActiveResult);
			if (isResultDifferentFromLastTime && !isCooldownResult)
			{
				_logger.LogInformation("Scale result: Outcome={Outcome} AgentsAdded={AgentsAdded} AgentsRemoved={AgentsRemoved} Message={Message}", 
					result.Outcome, result.AgentsAddedCount, result.AgentsAddedCount, result.Message);
			}
			
			span.SetAttribute("resultOutcome", result.Outcome.ToString());
			span.SetAttribute("resultAgentsAdded", result.AgentsAddedCount);
			span.SetAttribute("resultAgentsRemoved", result.AgentsRemovedCount);
			span.SetAttribute("resultOutcome", result.Message);

			await _poolCollection.TryUpdateAsync(
				pool,
				new UpdatePoolOptions
				{
					LastScaleUpTime = scaleOutTime,
					LastScaleDownTime = scaleInTime,
					LastScaleResult = isCooldownResult ? null : result,
					LastAgentCount = currentAgentCount,
					LastDesiredAgentCount = desiredAgentCount 
				}
				);

			return result;
		}
		
		internal IEnumerable<string> GetPropValues(string name)
		{
			DateTime now = _clock.UtcNow;
			return name switch
			{
				"timeUtcYear" => new List<string> { now.Year.ToString() }, // as 1 to 9999
				"timeUtcMonth" => new List<string> { now.Month.ToString() }, // as 1 to 12
				"timeUtcDay" => new List<string> { now.Day.ToString() }, // as 1 to 31
				"timeUtcDayOfWeek" => new List<string> { now.DayOfWeek.ToString(), now.DayOfWeek.ToString().ToLower() },
				"timeUtcHour" => new List<string> { now.Hour.ToString() }, // as 0 to 23
				"timeUtcMin" => new List<string> { now.Minute.ToString() }, // as 0 to 59
				"timeUtcSec" => new List<string> { now.Second.ToString() }, // as 0 to 59
				
				"dayOfWeek" => new List<string> { now.DayOfWeek.ToString().ToLower() }, // Deprecated, use timeUtcDayOfWeek
				_ => Array.Empty<string>()
			};
		}
		
		/// <summary>
		/// Instantiate a fleet manager using the list of conditions/configs in <see cref="IPool" />
		/// </summary>
		/// <param name="pool">Pool to use</param>
		/// <returns>A fleet manager with parameters set, as dictated by the pool argument</returns>
		/// <exception cref="ArgumentException">If fleet manager could not be instantiated</exception>
		public IFleetManager CreateFleetManager(IPool pool)
		{
			foreach (FleetManagerInfo info in pool.FleetManagers)
			{
				if (info.Condition == null || info.Condition.Evaluate(GetPropValues))
				{
					return _fleetManagerFactory.CreateFleetManager(info.Type, info.Config);
				}
			}

			return _fleetManagerFactory.CreateFleetManager(FleetManagerType.Default, "{}");
		}

		/// <summary>
		/// Instantiate a sizing strategy for the given pool
		/// Can resolve either from the list of strategies or the old legacy way
		/// </summary>
		/// <param name="pool">Pool to use</param>
		/// <returns>A pool sizing strategy with parameters set</returns>
		/// <exception cref="ArgumentException"></exception>
		public IPoolSizeStrategy CreatePoolSizeStrategy(IPool pool)
		{
			if (pool.SizeStrategies.Count > 0)
			{
				foreach (PoolSizeStrategyInfo info in pool.SizeStrategies)
				{
					if (info.Condition == null || info.Condition.Evaluate(GetPropValues))
					{
						switch (info.Type)
						{
							case PoolSizeStrategy.JobQueue:
								JobQueueSettings jqSettings = DeserializeConfig<JobQueueSettings>(info.Config);
								JobQueueStrategy jqStrategy = new (_jobCollection, _graphCollection, _streamCollection, _clock, _cache, _globalConfig, jqSettings);
								return info.ExtraAgentCount != 0 ? new ExtraAgentCountStrategy(jqStrategy, info.ExtraAgentCount) : jqStrategy;
							
							case PoolSizeStrategy.LeaseUtilization:
								LeaseUtilizationSettings luSettings = DeserializeConfig<LeaseUtilizationSettings>(info.Config);
								LeaseUtilizationStrategy luStrategy = new (_agentCollection, _poolCollection, _leaseCollection, _clock, _cache, luSettings);
								return info.ExtraAgentCount != 0 ? new ExtraAgentCountStrategy(luStrategy, info.ExtraAgentCount) : luStrategy;
							
							// Disabled until moved to a separate factory class as FleetService should not contain AWS-specific classes
							// case PoolSizeStrategy.ComputeQueueAwsMetric:
							// 	ComputeQueueAwsMetricSettings cqamSettings = DeserializeConfig<ComputeQueueAwsMetricSettings>(info.Config);
							// 	return new ComputeQueueAwsMetricStrategy(_awsCloudWatch, _computeService, cqamSettings);
							
							case PoolSizeStrategy.NoOp:
								NoOpPoolSizeStrategy noStrategy = new ();
								return info.ExtraAgentCount != 0 ? new ExtraAgentCountStrategy(noStrategy, info.ExtraAgentCount) : noStrategy;
							
							default:
								throw new ArgumentException("Invalid pool size strategy type " + info.Type);
						}
					}
				}
			}

			// These is the legacy way of creating and configuring strategies (list-based approach above is preferred)
			switch (pool.SizeStrategy ?? _settings.Value.DefaultAgentPoolSizeStrategy)
			{
				case PoolSizeStrategy.JobQueue:
					return new JobQueueStrategy(_jobCollection, _graphCollection, _streamCollection, _clock, _cache, _globalConfig, pool.JobQueueSettings);
				case PoolSizeStrategy.LeaseUtilization:
					LeaseUtilizationSettings luSettings = new();
					if (pool.MinAgents != null) luSettings.MinAgents = pool.MinAgents.Value;
					if (pool.NumReserveAgents != null) luSettings.NumReserveAgents = pool.NumReserveAgents.Value;
					return new LeaseUtilizationStrategy(_agentCollection, _poolCollection, _leaseCollection, _clock, _cache, luSettings);
				case PoolSizeStrategy.NoOp:
					return new NoOpPoolSizeStrategy();
				default:
					throw new ArgumentException("Unknown pool size strategy " + pool.SizeStrategy);
			}
		}

		/// <summary>
		/// </summary>
		
		
		/// <summary>
		/// Cancel a number of pending agent shutdowns for a pool
		/// 
		/// By preventing a shutdown, the agent can immediately be put back
		/// to service without requiring the provisioning of a new one
		/// </summary>
		/// <param name="pool">Pool to cancel shutdowns in</param>
		/// <param name="count">Number of shutdowns to cancel</param>
		/// <returns>Number of pending shutdown cancelled</returns>
		private async Task<int> CancelPendingShutdownsAsync(IPool pool, int count)
		{
			List<IAgent> agents = await _agentCollection.FindAsync(status: AgentStatus.Ok, enabled: true, poolId: pool.Id);
			int numShutdownsCancelled = 0;
			foreach (IAgent agent in agents)
			{
				if (agent.RequestShutdown && numShutdownsCancelled < count)
				{
					await _agentCollection.TryUpdateSettingsAsync(agent, requestShutdown: false);
					numShutdownsCancelled++;
				}
			}
			
			return numShutdownsCancelled;
		}

		private async Task<ScaleResult> ExpandWithPendingShutdownsFirstAsync(IPool pool, int agentsToAdd, Func<int, Task<ScaleResult>> scaleOutFunc)
		{
			int numShutdownsCancelled = await CancelPendingShutdownsAsync(pool, agentsToAdd);
			agentsToAdd -= numShutdownsCancelled;

			Activity.Current?.SetTag("numShutdownsCancelled", numShutdownsCancelled);

			if (agentsToAdd <= 0)
			{
				return new ScaleResult(FleetManagerOutcome.Success, numShutdownsCancelled, 0, "Scaled out by only cancelling shutdowns");
			}
			
			ScaleResult result = await scaleOutFunc(agentsToAdd);
			return new ScaleResult(result.Outcome, result.AgentsAddedCount + numShutdownsCancelled, result.AgentsRemovedCount, result.Message);
		}

		private static T DeserializeConfig<T>(string json)
		{
			json = String.IsNullOrEmpty(json) ? "{}" : json;
			T? config = JsonSerializer.Deserialize<T>(json, new JsonSerializerOptions { PropertyNameCaseInsensitive = true });
			if (config == null) throw new ArgumentException("Unable to deserialize config: " + json);
			return config;
		}
	}
}
