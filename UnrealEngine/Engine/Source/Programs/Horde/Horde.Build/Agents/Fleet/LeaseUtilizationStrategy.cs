// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Google.Protobuf.WellKnownTypes;
using Horde.Build.Agents.Leases;
using Horde.Build.Agents.Pools;
using Horde.Build.Utilities;
using HordeCommon;
using HordeCommon.Rpc.Tasks;
using OpenTracing;
using OpenTracing.Util;

namespace Horde.Build.Agents.Fleet
{
	using PoolId = StringId<IPool>;

	/// <summary>
	/// Lease utilization sizing settings for a pool
	/// </summary>
	public class LeaseUtilizationSettings
	{
	}
	
	/// <summary>
	/// Calculate pool size by looking at previously finished leases
	/// </summary>
	public class LeaseUtilizationStrategy : IPoolSizeStrategy
	{
		struct UtilizationSample
		{
			public double _jobWork;
			public double _otherWork;
		}

		class AgentData
		{
			public IAgent Agent { get; }
			public UtilizationSample[] Samples { get; }
			public List<ILease> Leases { get; } = new ();
			private readonly int _numSamples;

			public AgentData(IAgent agent, int numSamples)
			{
				Agent = agent;
				_numSamples = numSamples;
				Samples = new UtilizationSample[numSamples];
			}

			public void Add(double minT, double maxT, double jobWork, double otherWork)
			{
				int minIdx = Math.Clamp((int)minT, 0, _numSamples - 1);
				int maxIdx = Math.Clamp((int)maxT, minIdx, _numSamples - 1);
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
			public List<IAgent> Agents { get; } = new ();
			public UtilizationSample[] Samples { get; }

			public PoolData(IPool pool, int numSamples)
			{
				Pool = pool;
				Samples = new UtilizationSample[numSamples];
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
		
		private readonly IAgentCollection _agentCollection;
		private readonly IPoolCollection _poolCollection;
		private readonly ILeaseCollection _leaseCollection;
		private readonly IClock _clock;
		private readonly TimeSpan _sampleTime = TimeSpan.FromMinutes(6.0);
		private readonly int _numSamples = 10;
		private readonly int _numSamplesForResult = 9;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="agentCollection"></param>
		/// <param name="poolCollection"></param>
		/// <param name="leaseCollection"></param>
		/// <param name="clock"></param>
		/// <param name="numSamples">Number of samples to collect for calculating lease utilization</param>
		/// <param name="numSamplesForResult">Min number of samples for a valid result</param>
		/// <param name="sampleTime">Time period for each sample</param>
		public LeaseUtilizationStrategy(IAgentCollection agentCollection, IPoolCollection poolCollection, ILeaseCollection leaseCollection, IClock clock, int numSamples = 10, int numSamplesForResult = 9, TimeSpan? sampleTime = null)
		{
			_agentCollection = agentCollection;
			_poolCollection = poolCollection;
			_leaseCollection = leaseCollection;
			_clock = clock;
			_numSamples = numSamples;
			_numSamplesForResult = numSamplesForResult;
			_sampleTime = sampleTime ?? _sampleTime;
		}

		private async Task<Dictionary<AgentId, AgentData>> GetAgentDataAsync()
		{
			using IScope _ = GlobalTracer.Instance.BuildSpan("GetAgentDataAsync").StartActive();
			
			// Find all the current agents
			List<IAgent> agents = await _agentCollection.FindAsync(status: AgentStatus.Ok);

			// Query leases in last interval
			DateTime maxTime = _clock.UtcNow;
			DateTime minTime = maxTime - (_sampleTime * _numSamples);
			List<ILease> leases = await _leaseCollection.FindLeasesAsync(minTime, maxTime);

			// Add all the leases to a data object for each agent
			Dictionary<AgentId, AgentData> agentIdToData = agents.ToDictionary(x => x.Id, x => new AgentData(x, _numSamples));
			foreach (ILease lease in leases)
			{
				AgentData? agentData;
				if (agentIdToData.TryGetValue(lease.AgentId, out agentData) && agentData.Agent.SessionId == lease.SessionId)
				{
					agentData.Leases.Add(lease);
				}
			}
			
			// Compute utilization for each agent
			foreach (AgentData agentData in agentIdToData.Values)
			{
				foreach (ILease lease in agentData.Leases.OrderBy(x => x.StartTime))
				{
					double minT = (lease.StartTime - minTime).TotalSeconds / _sampleTime.TotalSeconds;
					double maxT = (lease.FinishTime == null)
						? _numSamples
						: ((lease.FinishTime.Value - minTime).TotalSeconds / _sampleTime.TotalSeconds);

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

			return agentIdToData;
		}
		
		private async Task<Dictionary<PoolId, PoolData>> GetPoolDataAsync()
		{
			using IScope _ = GlobalTracer.Instance.BuildSpan("GetPoolDataAsync").StartActive();

			Dictionary<AgentId, AgentData> agentIdToData = await GetAgentDataAsync();
			
			// Get all the pools
			List<IPool> pools = await _poolCollection.GetAsync();
			Dictionary<PoolId, PoolData> poolToData = pools.ToDictionary(x => x.Id, x => new PoolData(x, _numSamples));

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

			return poolToData;
		}

		/// <inheritdoc/>
		public string Name { get; } = "LeaseUtilization";

		/// <inheritdoc/>
		public async Task<List<PoolSizeData>> CalcDesiredPoolSizesAsync(List<PoolSizeData> pools)
		{
			Dictionary<PoolId, PoolData> poolToData = await GetPoolDataAsync();
			List<PoolSizeData> result = new();

			foreach (PoolData poolData in poolToData.Values.OrderByDescending(x => x.Agents.Count))
			{
				IPool pool = poolData.Pool;

				PoolSizeData? poolSize = pools.Find(x => x.Pool.Id == pool.Id);
				if (poolSize != null)
				{
					int minAgents = pool.MinAgents ?? 1;
					int numReserveAgents = pool.NumReserveAgents ?? 5;
					double utilization = poolData.Samples.Select(x => x._jobWork).OrderByDescending(x => x).Skip(_numSamples - _numSamplesForResult).First();
				
					// Number of agents in use over the sampling period. Can never be greater than number of agents available in pool.
					int numAgentsUtilized = (int)utilization;
					
					// Include reserve agent count to ensure pool always can grow
					int desiredAgentCount = Math.Max(numAgentsUtilized + numReserveAgents, minAgents);
					
					StringBuilder sb = new();
					sb.AppendFormat("Jobs=[{0}] ", GetDensityMap(poolData.Samples.Select(x => x._jobWork / Math.Max(1, poolData.Agents.Count))));
					sb.AppendFormat("Total=[{0}] ", GetDensityMap(poolData.Samples.Select(x => x._otherWork / Math.Max(1, poolData.Agents.Count))));
					sb.AppendFormat("Min=[{0,5:0.0}] ", poolData.Samples.Min(x => x._jobWork));
					sb.AppendFormat("Max=[{0,5:0.0}] ", poolData.Samples.Max(x => x._jobWork));
					sb.AppendFormat("Avg=[{0,5:0.0}] ", utilization);
					sb.AppendFormat("Pct=[{0,5:0.0}] ", poolData.Samples.Sum(x => x._jobWork) / _numSamples);

					result.Add(new(pool, poolSize.Agents, desiredAgentCount, sb.ToString()));
				}
			}

			return result;
		}
		
		/// <summary>
		/// Creates a string of characters indicating a sequence of 0-1 values over time
		/// </summary>
		/// <param name="values">Sequence of values</param>
		/// <returns>Density map</returns>
		private static string GetDensityMap(IEnumerable<double> values)
		{
			const string Greyscale = " 123456789";
			return new string(values.Select(x => Greyscale[Math.Clamp((int)(x * Greyscale.Length), 0, Greyscale.Length - 1)]).ToArray());
		}
	}
}
