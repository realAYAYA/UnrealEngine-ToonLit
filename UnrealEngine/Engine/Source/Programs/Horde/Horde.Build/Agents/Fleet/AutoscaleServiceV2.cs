// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Horde.Build.Agents.Pools;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using OpenTracing;
using OpenTracing.Util;
using StatsdClient;

namespace Horde.Build.Agents.Fleet
{
	using PoolId = StringId<IPool>;

	/// <summary>
	/// Service for managing the autoscaling of agent pools
	/// </summary>
	public sealed class AutoscaleServiceV2 : IHostedService, IDisposable
	{
		private IPoolSizeStrategy _leaseUtilizationStrategy;
		private IPoolSizeStrategy _jobQueueStrategy;
		private IPoolSizeStrategy _noOpStrategy;
		private IPoolSizeStrategy _computeQueueAwsMetricStrategy;
		private readonly IAgentCollection _agentCollection;
		private readonly IPoolCollection _poolCollection;
		private readonly IFleetManager _fleetManager;
		private readonly IDogStatsd _dogStatsd;
		private readonly IClock _clock;
		private readonly ITicker _ticker;
		private readonly ITicker _tickerHighFrequency;
		private readonly ServerSettings _settings;
		private readonly TimeSpan _defaultScaleOutCooldown;
		private readonly TimeSpan _defaultScaleInCooldown;
		private readonly ILogger<AutoscaleServiceV2> _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public AutoscaleServiceV2(
			LeaseUtilizationStrategy leaseUtilizationStrategy,
			JobQueueStrategy jobQueueStrategy,
			NoOpPoolSizeStrategy noOpStrategy,
			ComputeQueueAwsMetricStrategy computeQueueAwsMetricStrategy,
			IAgentCollection agentCollection,
			IPoolCollection poolCollection,
			IFleetManager fleetManager,
			IDogStatsd dogStatsd,
			IClock clock,
			IOptions<ServerSettings> settings,
			ILogger<AutoscaleServiceV2> logger)
		{
			_leaseUtilizationStrategy = leaseUtilizationStrategy;
			_jobQueueStrategy = jobQueueStrategy;
			_noOpStrategy = noOpStrategy;
			_computeQueueAwsMetricStrategy = computeQueueAwsMetricStrategy;
			_agentCollection = agentCollection;
			_poolCollection = poolCollection;
			_fleetManager = fleetManager;
			_dogStatsd = dogStatsd;
			_clock = clock;
			_ticker = clock.AddSharedTicker<AutoscaleServiceV2>(TimeSpan.FromMinutes(5.0), TickLeaderAsync, logger);
			_tickerHighFrequency = clock.AddSharedTicker("AutoscaleServiceV2.TickHighFrequency", TimeSpan.FromSeconds(30), TickHighFrequencyAsync, logger);
			_settings = settings.Value;
			_logger = logger;
			_defaultScaleOutCooldown = TimeSpan.FromSeconds(settings.Value.AgentPoolScaleOutCooldownSeconds);
			_defaultScaleInCooldown = TimeSpan.FromSeconds(settings.Value.AgentPoolScaleInCooldownSeconds);
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
			_logger.LogInformation("Autoscaling pools...");
			Stopwatch stopwatch = Stopwatch.StartNew();
			using IScope _ = GlobalTracer.Instance.BuildSpan("AutoscaleService.TickAsync").StartActive();
			
			await BatchResizePools(false);

			stopwatch.Stop();
			_logger.LogInformation("Autoscaling pools took {ElapsedTime} ms", stopwatch.ElapsedMilliseconds);
		}

		internal async ValueTask TickHighFrequencyAsync(CancellationToken stoppingToken)
		{
			_logger.LogInformation("Autoscaling pools (high frequency)...");
			Stopwatch stopwatch = Stopwatch.StartNew();
			using IScope _ = GlobalTracer.Instance.BuildSpan("AutoscaleService.TickHighFrequency").StartActive();
			
			await BatchResizePools(true);
			
			stopwatch.Stop();
			_logger.LogInformation("Autoscaling pools (high frequency) took {ElapsedTime} ms", stopwatch.ElapsedMilliseconds);
		}
		
		private async Task BatchResizePools(bool onlyHighFrequency)
		{
			// Group pools by strategy type to ensure they can be called once (more optimal)
			Dictionary<PoolSizeStrategy, List<PoolSizeData>> poolSizeDataByStrategy = await GetPoolSizeDataByStrategyType();

			if (onlyHighFrequency)
			{
				poolSizeDataByStrategy = poolSizeDataByStrategy
					.Where(x => x.Key.SupportsHighFrequency())
					.ToDictionary(i => i.Key, i => i.Value);
			}
			
			foreach ((PoolSizeStrategy strategyType, List<PoolSizeData> currentData) in poolSizeDataByStrategy)
			{
				List<PoolSizeData> newData = await GetPoolSizeStrategy(strategyType).CalcDesiredPoolSizesAsync(currentData);
				await ResizePools(newData);
			}
		}

		internal async Task<Dictionary<PoolSizeStrategy, List<PoolSizeData>>> GetPoolSizeDataByStrategyType()
		{
			List<IAgent> agents = await _agentCollection.FindAsync(status: AgentStatus.Ok, enabled: true);
			List<IAgent> GetAgentsInPool(PoolId poolId) => agents.FindAll(a => a.GetPools().Any(p => p == poolId));
			List<IPool> pools = await _poolCollection.GetAsync();

			Dictionary<PoolSizeStrategy, List<PoolSizeData>> result = new();
			foreach (PoolSizeStrategy strategyType in Enum.GetValues<PoolSizeStrategy>())
			{
				result[strategyType] = pools
					.Where(x => x.SizeStrategy == strategyType)
					.Select(x => new PoolSizeData(x, GetAgentsInPool(x.Id), null))
					.ToList();
			}

			return result;
		}

		internal async Task ResizePools(List<PoolSizeData> poolSizeDatas)
		{
			foreach (PoolSizeData poolSizeData in poolSizeDatas.OrderByDescending(x => x.Agents.Count))
			{
				IPool pool = poolSizeData.Pool;

				if (!pool.EnableAutoscaling || poolSizeData.DesiredAgentCount == null)
				{
					continue;
				}

				int currentAgentCount = poolSizeData.Agents.Count;
				int desiredAgentCount = poolSizeData.DesiredAgentCount.Value;
				int deltaAgentCount = desiredAgentCount - currentAgentCount;

				_logger.LogInformation("{PoolName,-48} Current={Current,4} Target={Target,4} Delta={Delta,4} Status={Status}", pool.Name, currentAgentCount, desiredAgentCount, deltaAgentCount, poolSizeData.StatusMessage);
				
				try
				{
					using IScope scope = GlobalTracer.Instance.BuildSpan("ScalingPool").StartActive();
					scope.Span.SetTag("poolName", pool.Name);
					scope.Span.SetTag("current", currentAgentCount);
					scope.Span.SetTag("desired", desiredAgentCount);
					scope.Span.SetTag("delta", deltaAgentCount);

					if (deltaAgentCount > 0)
					{
						TimeSpan scaleOutCooldown = pool.ScaleOutCooldown ?? _defaultScaleOutCooldown;
						bool isCoolingDown = pool.LastScaleUpTime != null && pool.LastScaleUpTime + scaleOutCooldown > _clock.UtcNow;
						scope.Span.SetTag("isCoolingDown", isCoolingDown);
						if (!isCoolingDown)
						{
							await _fleetManager.ExpandPoolAsync(pool, poolSizeData.Agents, deltaAgentCount);
							await _poolCollection.TryUpdateAsync(pool, lastScaleUpTime: _clock.UtcNow);
						}
						else
						{
							TimeSpan? cooldownTimeLeft = pool.LastScaleUpTime + _defaultScaleOutCooldown - _clock.UtcNow;
							_logger.LogDebug("Cannot scale out {PoolName}, it's cooling down for another {TimeLeft} secs", pool.Name, cooldownTimeLeft?.TotalSeconds);
						}
					}

					if (deltaAgentCount < 0)
					{
						TimeSpan scaleInCooldown = pool.ScaleInCooldown ?? _defaultScaleInCooldown;
						bool isCoolingDown = pool.LastScaleDownTime != null && pool.LastScaleDownTime + scaleInCooldown > _clock.UtcNow;
						scope.Span.SetTag("isCoolingDown", isCoolingDown);
						if (!isCoolingDown)
						{
							await _fleetManager.ShrinkPoolAsync(pool, poolSizeData.Agents, -deltaAgentCount);
							await _poolCollection.TryUpdateAsync(pool, lastScaleDownTime: _clock.UtcNow);
						}
						else
						{
							TimeSpan? cooldownTimeLeft = pool.LastScaleDownTime + _defaultScaleInCooldown - _clock.UtcNow;
							_logger.LogDebug("Cannot scale in {PoolName}, it's cooling down for another {TimeLeft} secs", pool.Name, cooldownTimeLeft?.TotalSeconds);
						}
					}
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Failed to scale {PoolName}:\n{Exception}", pool.Name, ex);
					continue;
				}

				_dogStatsd.Gauge("agentpools.autoscale.target", desiredAgentCount, tags: new []{"pool:" + pool.Name});
				_dogStatsd.Gauge("agentpools.autoscale.current", currentAgentCount, tags: new []{"pool:" + pool.Name});
			}
		}

		/// <summary>
		/// Backdoor for tests to override strategies with test doubles
		/// These cannot supplied in the constructor since the DI requires concrete implementations.
		/// </summary>
		/// <param name="leaseUtilizationStrategy"></param>
		/// <param name="jobQueueStrategy"></param>
		/// <param name="noOpStrategy"></param>
		/// <param name="computeQueueAwsMetricStrategy"></param>
		internal void OverridePoolSizeStrategiesDuringTesting(
			IPoolSizeStrategy leaseUtilizationStrategy,
			IPoolSizeStrategy jobQueueStrategy,
			IPoolSizeStrategy noOpStrategy,
			IPoolSizeStrategy computeQueueAwsMetricStrategy)
		{
			_leaseUtilizationStrategy = leaseUtilizationStrategy;
			_jobQueueStrategy = jobQueueStrategy;
			_noOpStrategy = noOpStrategy;
			_computeQueueAwsMetricStrategy = computeQueueAwsMetricStrategy;
		}
		
		private IPoolSizeStrategy GetPoolSizeStrategy(PoolSizeStrategy type)
		{
			return type switch
			{
				PoolSizeStrategy.LeaseUtilization => _leaseUtilizationStrategy,
				PoolSizeStrategy.JobQueue => _jobQueueStrategy,
				PoolSizeStrategy.NoOp => _noOpStrategy,
				PoolSizeStrategy.ComputeQueueAwsMetric => _computeQueueAwsMetricStrategy,
				_ => throw new Exception($"Unknown strategy: {type}")
			};
		}
	}
}
