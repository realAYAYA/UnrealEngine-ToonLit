// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Horde.Build.Agents.Pools;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Streams;
using Horde.Build.Utilities;
using HordeCommon;

namespace Horde.Build.Agents.Fleet
{
	using PoolId = StringId<IPool>;
	using StreamId = StringId<IStream>;

	/// <summary>
	/// Job queue sizing settings for a pool
	/// </summary>
	public class JobQueueSettings
	{
		/// <summary>
		/// Factor translating queue size to additional agents to grow the pool with
		/// The result is always rounded up to nearest integer. 
		/// Example: if there are 20 jobs in queue, a factor 0.25 will result in 5 new agents being added (20 * 0.25)
		/// </summary>
		public double ScaleOutFactor { get; set;  } = 0.25;
		
		/// <summary>
		/// Factor by which to shrink the pool size with when queue is empty
		/// The result is always rounded up to nearest integer.
		/// Example: when the queue size is zero, a default value of 0.9 will shrink the pool by 10% (current agent count * 0.9)
		/// </summary>
		public double ScaleInFactor { get; set; } = 0.9;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="scaleOutFactor"></param>
		/// <param name="scaleInFactor"></param>
		public JobQueueSettings(double? scaleOutFactor = null, double? scaleInFactor = null)
		{
			ScaleOutFactor = scaleOutFactor.GetValueOrDefault(ScaleOutFactor);
			ScaleInFactor = scaleInFactor.GetValueOrDefault(ScaleInFactor);
		}
	}
	
	/// <summary>
	/// Calculate pool size by observing the number of jobs in waiting state
	///
	/// Allows for more proactive scaling compared to LeaseUtilizationStrategy.
	/// <see cref="LeaseUtilizationStrategy"/> 
	/// </summary>
	public class JobQueueStrategy : IPoolSizeStrategy
	{
		private readonly IJobCollection _jobs;
		private readonly IGraphCollection _graphs;
		private readonly StreamService _streamService;
		private readonly IClock _clock;
		
		/// <summary>
		/// How far back in time to look for job batches (that potentially are in the queue)
		/// </summary>
		private readonly TimeSpan _samplePeriod = TimeSpan.FromDays(5);
		
		/// <summary>
		/// Time spent in ready state before considered truly waiting for an agent
		///
		/// A job batch can be in ready state before getting picked up and executed.
		/// This threshold will help ensure only batches that have been waiting longer than this value will be considered.
		/// </summary>
		internal readonly TimeSpan ReadyTimeThreshold = TimeSpan.FromSeconds(45.0);

		private readonly JobQueueSettings _defaultPoolSettings = new();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="jobs"></param>
		/// <param name="graphs"></param>
		/// <param name="streamService"></param>
		/// <param name="clock"></param>
		/// <param name="samplePeriod">Time period for each sample</param>
		public JobQueueStrategy(IJobCollection jobs, IGraphCollection graphs, StreamService streamService, IClock clock, TimeSpan? samplePeriod = null)
		{
			_jobs = jobs;
			_graphs = graphs;
			_streamService = streamService;
			_clock = clock;
			_samplePeriod = samplePeriod ?? _samplePeriod;
		}

		/// <inheritdoc/>
		public string Name { get; } = "JobQueue";

		/// <summary>
		/// Extract all job step batches from a job, with their associated pool 
		/// </summary>
		/// <param name="job">Job to extract from</param>
		/// <param name="streams">Cached lookup table of streams</param>
		/// <returns></returns>
		private async Task<List<(IJob Job, IJobStepBatch Batch, PoolId PoolId)>> GetJobBatchesWithPools(IJob job, Dictionary<StreamId, IStream> streams)
		{
			IGraph graph = await _graphs.GetAsync(job.GraphHash);

			List<(IJob Job, IJobStepBatch Batch, PoolId PoolId)> jobBatches = new();
			foreach (IJobStepBatch batch in job.Batches)
			{
				if (batch.State != JobStepBatchState.Ready)
				{
					continue;
				}
				
				TimeSpan? waitTime = _clock.UtcNow - batch.ReadyTimeUtc;
				if (waitTime == null)
				{
					continue;
				}
				if (waitTime.Value < ReadyTimeThreshold)
				{
					continue;
				}

				if (!streams.TryGetValue(job.StreamId, out IStream? stream))
				{
					continue;
				}

				string batchAgentType = graph.Groups[batch.GroupIdx].AgentType;
				if (!stream.AgentTypes.TryGetValue(batchAgentType, out AgentType? agentType))
				{
					continue;
				}
				
				jobBatches.Add((job, batch, agentType.Pool));
			}

			return jobBatches;
		}

		internal async Task<Dictionary<PoolId, int>> GetPoolQueueSizesAsync(DateTimeOffset jobsCreatedAfter)
		{
			List<IStream> streamsList = await _streamService.GetStreamsAsync();
			Dictionary<StreamId, IStream> streams = streamsList.ToDictionary(x => x.Id, x => x);
			List<IJob> recentJobs = await _jobs.FindAsync(minCreateTime: jobsCreatedAfter);

			List<(IJob Job, IJobStepBatch Batch, PoolId PoolId)> jobBatches = new();
			foreach (IJob job in recentJobs)
			{
				jobBatches.AddRange(await GetJobBatchesWithPools(job, streams));
			}

			List<(PoolId PoolId, int QueueSize)> poolsWithQueueSize = jobBatches.GroupBy(t => t.PoolId).Select(t => (t.Key, t.Count())).ToList();
			return poolsWithQueueSize.ToDictionary(x => x.PoolId, x => x.QueueSize);
		}

		/// <inheritdoc/>
		public async Task<List<PoolSizeData>> CalcDesiredPoolSizesAsync(List<PoolSizeData> pools)
		{
			DateTimeOffset minCreateTime = _clock.UtcNow - _samplePeriod;
			Dictionary<PoolId, int> poolQueueSizes = await GetPoolQueueSizesAsync(minCreateTime);

			return pools.Select(current =>
			{
				JobQueueSettings settings = current.Pool.JobQueueSettings ?? _defaultPoolSettings;
				poolQueueSizes.TryGetValue(current.Pool.Id, out int queueSize);
				if (queueSize > 0)
				{
					int additionalAgentCount = (int)Math.Ceiling(queueSize * settings.ScaleOutFactor);
					int desiredAgentCount = current.Agents.Count + additionalAgentCount;
					return new PoolSizeData(current.Pool, current.Agents, desiredAgentCount, $"QueueSize={queueSize}");
				}
				else
				{
					int desiredAgentCount = (int)(current.Agents.Count * settings.ScaleInFactor);
					return new PoolSizeData(current.Pool, current.Agents, desiredAgentCount, "Empty job queue");
				}
			}).ToList();
		}
	}
}
