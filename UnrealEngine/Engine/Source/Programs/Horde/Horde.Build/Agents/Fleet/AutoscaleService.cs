// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Google.Protobuf.WellKnownTypes;
using Horde.Build.Agents.Leases;
using Horde.Build.Agents.Pools;
using Horde.Build.Server;
using Horde.Build.Utilities;
using HordeCommon;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using OpenTracing;
using OpenTracing.Util;
using StatsdClient;

namespace Horde.Build.Agents.Fleet
{
	using PoolId = StringId<IPool>;

	/// <summary>
	/// Service for managing the autoscaling of agent pools
	/// </summary>
	public sealed class AutoscaleService : IHostedService, IDisposable
	{
		const int NumSamples = 10;
		const int NumSamplesForResult = 9;
		static readonly TimeSpan s_sampleTime = TimeSpan.FromMinutes(6.0);
		static readonly TimeSpan s_shrinkPoolCoolDown = TimeSpan.FromMinutes(20.0);

		struct UtilizationSample
		{
			public double _jobWork;
			public double _otherWork;
		}

		class AgentData
		{
			public IAgent Agent { get; }
			public UtilizationSample[] Samples { get; } = new UtilizationSample[NumSamples];
			public List<ILease> Leases { get; } = new List<ILease>();

			public AgentData(IAgent agent)
			{
				Agent = agent;
			}

			public void Add(double minT, double maxT, double jobWork, double otherWork)
			{
				int minIdx = Math.Clamp((int)minT, 0, NumSamples - 1);
				int maxIdx = Math.Clamp((int)maxT, minIdx, NumSamples - 1);
				for (int idx = minIdx; idx <= maxIdx; idx++)
				{
					double fraction = Math.Clamp(maxT - idx, 0.0, 1.0) - Math.Clamp(minT - idx, 0.0, 1.0);
					Samples[idx]._jobWork += jobWork * fraction;
					Samples[idx]._otherWork += otherWork * fraction;
				}
			}
		}

		class PoolData
		{
			public IPool Pool { get; }
			public List<IAgent> Agents { get; } = new List<IAgent>();
			public UtilizationSample[] Samples { get; } = new UtilizationSample[NumSamples];

			public PoolData(IPool pool)
			{
				Pool = pool;
			}

			public void Add(AgentData agentData)
			{
				Agents.Add(agentData.Agent);
				for (int idx = 0; idx < Samples.Length; idx++)
				{
					Samples[idx]._jobWork += agentData.Samples[idx]._jobWork;
					Samples[idx]._otherWork += agentData.Samples[idx]._otherWork;
				}
			}
		}

		readonly IAgentCollection _agentCollection;
		readonly IPoolCollection _poolCollection;
		readonly ILeaseCollection _leaseCollection;
		readonly IFleetManager _fleetManager;
		readonly IDogStatsd _dogStatsd;
		readonly IClock _clock;
		readonly ITicker _ticker;
		readonly ILogger<AutoscaleService> _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public AutoscaleService(IAgentCollection agentCollection, IPoolCollection poolCollection, ILeaseCollection leaseCollection, IFleetManager fleetManager, IDogStatsd dogStatsd, MongoService mongoService, IClock clock, ILogger<AutoscaleService> logger)
		{
			_agentCollection = agentCollection;
			_poolCollection = poolCollection;
			_leaseCollection = leaseCollection;
			_fleetManager = fleetManager;
			_dogStatsd = dogStatsd;
			_clock = clock;
			if (mongoService.ReadOnlyMode)
			{
				_ticker = new NullTicker();
			}
			else
			{
				_ticker = clock.AddSharedTicker<AutoscaleService>(TimeSpan.FromMinutes(5.0), TickLeaderAsync, logger);
			}
			_logger = logger;
		}

		/// <inheritdoc/>
		public Task StartAsync(CancellationToken cancellationToken) => _ticker.StartAsync();

		/// <inheritdoc/>
		public Task StopAsync(CancellationToken cancellationToken) => _ticker.StopAsync();

		/// <inheritdoc/>
		public void Dispose() => _ticker.Dispose();

		/// <inheritdoc/>
		async ValueTask TickLeaderAsync(CancellationToken stoppingToken)
		{
			DateTime utcNow = _clock.UtcNow;

			_logger.LogInformation("Autoscaling pools...");
			Stopwatch stopwatch = Stopwatch.StartNew();

			Dictionary<PoolId, PoolData> poolToData;
			using (IScope scope = GlobalTracer.Instance.BuildSpan("Compute utilization").StartActive())
			{
				// Find all the current agents
				List<IAgent> agents = await _agentCollection.FindAsync(status: AgentStatus.Ok);

				// Query leases in last interval
				DateTime maxTime = _clock.UtcNow;
				DateTime minTime = maxTime - (s_sampleTime * NumSamples);
				List<ILease> leases = await _leaseCollection.FindLeasesAsync(minTime, maxTime);

				// Add all the leases to a data object for each agent
				Dictionary<AgentId, AgentData> agentIdToData = agents.ToDictionary(x => x.Id, x => new AgentData(x));
				foreach (ILease lease in leases)
				{
					AgentData? agentData;
					if (agentIdToData.TryGetValue(lease.AgentId, out agentData) &&
					    agentData.Agent.SessionId == lease.SessionId)
					{
						agentData.Leases.Add(lease);
					}
				}

				// Compute utilization for each agent
				foreach (AgentData agentData in agentIdToData.Values)
				{
					foreach (ILease lease in agentData.Leases.OrderBy(x => x.StartTime))
					{
						double minT = (lease.StartTime - minTime).TotalSeconds / s_sampleTime.TotalSeconds;
						double maxT = (lease.FinishTime == null)
							? NumSamples
							: ((lease.FinishTime.Value - minTime).TotalSeconds / s_sampleTime.TotalSeconds);

						Any payload = Any.Parser.ParseFrom(lease.Payload.ToArray());
						if (payload.Is(ExecuteJobTask.Descriptor))
						{
							agentData.Add(minT, maxT, 1.0, 0.0);
						}
						else
						{
							agentData.Add(minT, maxT, 0.0, 1.0);
						}
					}
				}

				// Get all the pools
				List<IPool> pools = await _poolCollection.GetAsync();
				poolToData = pools.ToDictionary(x => x.Id, x => new PoolData(x));

				// Find pool utilization over the query period
				foreach (AgentData agentData in agentIdToData.Values)
				{
					foreach (PoolId poolId in agentData.Agent.GetPools())
					{
						PoolData? poolData;
						if (poolToData.TryGetValue(poolId, out poolData))
						{
							poolData.Add(agentData);
						}
					}
				}
			}

			// Output all the final stats
			foreach (PoolData poolData in poolToData.Values.OrderByDescending(x => x.Agents.Count))
			{
				IPool pool = poolData.Pool;

				int minAgents = pool.MinAgents ?? 1;
				int numReserveAgents = pool.NumReserveAgents ?? 5;

				double utilization = poolData.Samples.Select(x => x._jobWork).OrderByDescending(x => x).Skip(NumSamples - NumSamplesForResult).First();

				int targetAgents = Math.Max((int)utilization + numReserveAgents, minAgents);
				int delta = targetAgents - poolData.Agents.Count;

				_logger.LogInformation("{PoolName,-48} Jobs=[{JobUtilization}] Total=[{OtherUtilization}] Min={Min,5:0.0} Max={Max,5:0.0} Avg={Avg,5:0.0} Pct={Pct,5:0.0} Current={Current,4} Target={Target,4} Delta={Delta}",
					pool.Name,
					GetDensityMap(poolData.Samples.Select(x => x._jobWork / Math.Max(1, poolData.Agents.Count))),
					GetDensityMap(poolData.Samples.Select(x => x._otherWork / Math.Max(1, poolData.Agents.Count))),
					poolData.Samples.Min(x => x._jobWork),
					poolData.Samples.Max(x => x._jobWork),
					utilization,
					poolData.Samples.Sum(x => x._jobWork) / NumSamples,
					poolData.Agents.Count,
					targetAgents,
					delta);

				try
				{
					if (pool.EnableAutoscaling)
					{
						using (IScope scope = GlobalTracer.Instance.BuildSpan("Scaling pool").StartActive())
						{
							scope.Span.SetTag("poolName", pool.Name);
							scope.Span.SetTag("delta", delta);

							if (delta > 0)
							{
								await _fleetManager.ExpandPoolAsync(pool, poolData.Agents, delta);
								await _poolCollection.TryUpdateAsync(pool, lastScaleUpTime: DateTime.UtcNow);
							}

							if (delta < 0)
							{
								bool bShrinkIsOnCoolDown = pool.LastScaleDownTime != null && pool.LastScaleDownTime + s_shrinkPoolCoolDown > DateTime.UtcNow;
								if (!bShrinkIsOnCoolDown)
								{
									await _fleetManager.ShrinkPoolAsync(pool, poolData.Agents, -delta);
									await _poolCollection.TryUpdateAsync(pool, lastScaleDownTime: DateTime.UtcNow);
								}
								else
								{
									_logger.LogDebug("Cannot shrink {PoolName} right now, it's on cool-down until {CoolDownTimeEnds}", pool.Name, pool.LastScaleDownTime + s_shrinkPoolCoolDown);
								}
							}
						}
					}
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Failed to scale {PoolName}:\n{Exception}", pool.Name, ex);
					continue;
				}

				_dogStatsd.Gauge("agentpools.autoscale.target", targetAgents, tags: new []{"pool:" + pool.Name});
				_dogStatsd.Gauge("agentpools.autoscale.current", poolData.Agents.Count, tags: new []{"pool:" + pool.Name});
			}
			
			stopwatch.Stop();
			_logger.LogInformation("Autoscaling pools took {ElapsedTime} ms", stopwatch.ElapsedMilliseconds);
		}

		/// <summary>
		/// Creates a string of characters indicating a sequence of 0-1 values over time
		/// </summary>
		/// <param name="values">Sequence of values</param>
		/// <returns>Density map</returns>
		static string GetDensityMap(IEnumerable<double> values)
		{
			const string Greyscale = " 123456789";
			return new string(values.Select(x => Greyscale[Math.Clamp((int)(x * Greyscale.Length), 0, Greyscale.Length - 1)]).ToArray());
		}
	}
}
