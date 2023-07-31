// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Horde.Build.Agents.Fleet;
using Horde.Build.Agents.Leases;
using Horde.Build.Agents.Pools;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace Horde.Build.Agents.Telemetry
{
	using PoolId = StringId<IPool>;

	/// <summary>
	/// Service which updates telemetry periodically
	/// </summary>
	public sealed class TelemetryService : IHostedService, IDisposable
	{
		readonly ITelemetryCollection _telemetryCollection;
		readonly IAgentCollection _agentCollection;
		readonly ILeaseCollection _leaseCollection;
		readonly IPoolCollection _poolCollection;
		readonly IFleetManager _fleetManager;
		readonly IClock _clock;
		readonly ITicker _tick;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public TelemetryService(ITelemetryCollection telemetryCollection, IAgentCollection agentCollection, ILeaseCollection leaseCollection, IPoolCollection poolCollection, IFleetManager fleetManager, IClock clock, ILogger<TelemetryService> logger)
		{
			_telemetryCollection = telemetryCollection;
			_agentCollection = agentCollection;
			_leaseCollection = leaseCollection;
			_poolCollection = poolCollection;
			_fleetManager = fleetManager;
			_clock = clock;
			_tick = clock.AddSharedTicker<TelemetryService>(TimeSpan.FromMinutes(10.0), TickLeaderAsync, logger);
			_logger = logger;
		}

		/// <inheritdoc/>
		public Task StartAsync(CancellationToken cancellationToken) => _tick.StartAsync();

		/// <inheritdoc/>
		public Task StopAsync(CancellationToken cancellationToken) => _tick.StopAsync();

		/// <inheritdoc/>
		public void Dispose() => _tick.Dispose();

		/// <inheritdoc/>
		async ValueTask TickLeaderAsync(CancellationToken stoppingToken)
		{
			DateTime currentTime = _clock.UtcNow;

			// Find the last time that we need telemetry for
			DateTime maxTime = currentTime.Date + TimeSpan.FromHours(currentTime.Hour);

			// Get the latest telemetry data
			IUtilizationTelemetry? latest = await _telemetryCollection.GetLatestUtilizationTelemetryAsync();
			TimeSpan interval = TimeSpan.FromHours(1.0);
			int count = (latest == null) ? (7 * 24) : (int)Math.Round((maxTime - latest.FinishTime) / interval);
			DateTime minTime = maxTime - count * interval;

			// Query all the current data
			List<IAgent> agents = await _agentCollection.FindAsync();
			List<IPool> pools = await _poolCollection.GetAsync();
			List<ILease> leases = await _leaseCollection.FindLeasesAsync(minTime: minTime);

			// Remove any agents which are offline
			agents.RemoveAll(x => !x.Enabled || !x.IsSessionValid(currentTime));

			// Find all the agents
			Dictionary<AgentId, List<PoolId>> agentToPoolIds = agents.ToDictionary(x => x.Id, x => x.GetPools().ToList());

			// Generate all the telemetry data
			DateTime bucketMinTime = minTime;
			for (int idx = 0; idx < count; idx++)
			{
				DateTime bucketMaxTime = bucketMinTime + interval;
				_logger.LogInformation("Creating telemetry for {MinTime} to {MaxTime}", bucketMinTime, bucketMaxTime);

				NewUtilizationTelemetry telemetry = new NewUtilizationTelemetry(bucketMinTime, bucketMaxTime);
				telemetry.NumAgents = agents.Count;
				foreach (IPool pool in pools)
				{
					if (pool.EnableAutoscaling)
					{
						int numStoppedInstances = await _fleetManager.GetNumStoppedInstancesAsync(pool);
						telemetry.NumAgents += numStoppedInstances;

						NewPoolUtilizationTelemetry poolTelemetry = telemetry.FindOrAddPool(pool.Id);
						poolTelemetry.NumAgents += numStoppedInstances;
						poolTelemetry.HibernatingTime += interval.TotalHours * numStoppedInstances;
					}
				}
				foreach (PoolId poolId in agentToPoolIds.Values.SelectMany(x => x))
				{
					telemetry.FindOrAddPool(poolId).NumAgents++;
				}
				foreach (ILease lease in leases)
				{
					if (lease.StartTime < bucketMaxTime && (!lease.FinishTime.HasValue || lease.FinishTime >= bucketMinTime))
					{
						List<PoolId>? leasePools;
						if (agentToPoolIds.TryGetValue(lease.AgentId, out leasePools))
						{
							DateTime finishTime = lease.FinishTime ?? bucketMaxTime;
							double time = new TimeSpan(Math.Min(finishTime.Ticks, bucketMaxTime.Ticks) - Math.Max(lease.StartTime.Ticks, bucketMinTime.Ticks)).TotalHours;

							foreach (PoolId poolId in leasePools)
							{
								NewPoolUtilizationTelemetry poolTelemetry = telemetry.FindOrAddPool(poolId);
								if (lease.PoolId == null || lease.StreamId == null)
								{
									poolTelemetry.AdminTime += time;
								}
								else if (poolId == lease.PoolId)
								{
									poolTelemetry.FindOrAddStream(lease.StreamId.Value).Time += time;
								}
								else
								{
									poolTelemetry.OtherTime += time;
								}
							}
						}
					}
				}
				await _telemetryCollection.AddUtilizationTelemetryAsync(telemetry);

				bucketMinTime = bucketMaxTime;
			}
		}
	}
}
