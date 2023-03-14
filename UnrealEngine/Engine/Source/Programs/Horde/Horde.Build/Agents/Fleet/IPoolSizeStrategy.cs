// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Horde.Build.Agents.Pools;

namespace Horde.Build.Agents.Fleet
{
	/// <summary>
	/// Available pool sizing strategies
	/// </summary>
	public enum PoolSizeStrategy
	{
		/// <summary>
		/// Strategy based on lease utilization
		/// <see cref="LeaseUtilizationStrategy"/>
		/// </summary>
		LeaseUtilization,
		
		/// <summary>
		/// Strategy based on size of job build queue
		/// <see cref="JobQueueStrategy"/> 
		/// </summary>
		JobQueue,
		
		/// <summary>
		/// No-op strategy used as fallback/default behavior
		/// <see cref="NoOpPoolSizeStrategy"/> 
		/// </summary>
		NoOp,
		
		/// <summary>
		/// A no-op strategy that reports metrics to let an external AWS auto-scaling policy scale the fleet
		/// <see cref="ComputeQueueAwsMetric"/> 
		/// </summary>
		ComputeQueueAwsMetric
	}
	
	/// <summary>
	/// Extensions for pool size strategy
	/// </summary>
	public static class PoolSizeStrategyExtensions
	{
		/// <summary>
		/// Checks whether the strategy can be called more often than normal (i.e it's cheap to run)
		/// </summary>
		/// <param name="strategy"></param>
		/// <returns>True if it supports high-frequency</returns>
		public static bool SupportsHighFrequency(this PoolSizeStrategy strategy)
		{
			return strategy switch
			{
				PoolSizeStrategy.ComputeQueueAwsMetric => true,
				_ => false
			};
		}
	}

	/// <summary>
	/// Class for specifying and grouping data together required for calculating pool size
	/// </summary>
	public class PoolSizeData
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
		/// The desired agent count (calculated and updated once the strategy has been run, null otherwise)
		/// </summary>
		public int? DesiredAgentCount  { get; }
		
		/// <summary>
		/// Human-readable text describing the status of the pool (data the sizing is based on etc)
		/// </summary>
		public string StatusMessage { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="pool"></param>
		/// <param name="agents"></param>
		/// <param name="desiredAgentCount"></param>
		/// <param name="statusMessage"></param>
		public PoolSizeData(IPool pool, List<IAgent> agents, int? desiredAgentCount, string statusMessage = "N/A")
		{
			Pool = pool;
			Agents = agents;
			DesiredAgentCount = desiredAgentCount;
			StatusMessage = statusMessage;
		}

		/// <summary>
		/// Copy the object, inheriting any unspecified values from current instance
		/// Needed because the class is immutable 
		/// </summary>
		/// <param name="pool"></param>
		/// <param name="agents"></param>
		/// <param name="desiredAgentCount"></param>
		/// <param name="statusMessage"></param>
		/// <returns>A new copy</returns>
		public PoolSizeData Copy(IPool? pool = null, List<IAgent>? agents = null, int? desiredAgentCount = null, string? statusMessage = null)
		{
			return new PoolSizeData(pool ?? Pool, agents ?? Agents, desiredAgentCount ?? DesiredAgentCount, statusMessage ?? StatusMessage);
		}
	}
	
	/// <summary>
	/// Interface for different agent pool sizing strategies
	/// </summary>
	public interface IPoolSizeStrategy
	{
		/// <summary>
		/// Calculate the adequate number of agents to be online for given pools
		/// </summary>
		/// <param name="pools">Pools including attached agents</param>
		/// <returns></returns>
		Task<List<PoolSizeData>> CalcDesiredPoolSizesAsync(List<PoolSizeData> pools);
		
		/// <summary>
		/// Name of the strategy
		/// </summary>
		string Name { get; }
	}
	
	/// <summary>
	/// No-operation strategy that won't resize pools, just return the existing count.
	/// Used to ensure there's always a strategy available for dependency injection, even if it does nothing.
	/// </summary>
	public class NoOpPoolSizeStrategy : IPoolSizeStrategy
	{
		/// <inheritdoc/>
		public Task<List<PoolSizeData>> CalcDesiredPoolSizesAsync(List<PoolSizeData> pools)
		{
			List<PoolSizeData> result = pools.Select(x => new PoolSizeData(x.Pool, x.Agents, x.Agents.Count, "(no-op)")).ToList();
			return Task.FromResult(result);
		}

		/// <inheritdoc/>
		public string Name { get; } = "NoOp";
	}
}
