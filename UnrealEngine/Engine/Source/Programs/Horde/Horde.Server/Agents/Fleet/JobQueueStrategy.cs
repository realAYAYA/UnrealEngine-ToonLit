// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Agents.Pools;
using EpicGames.Horde.Streams;
using Horde.Server.Agents.Pools;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Server;
using Horde.Server.Streams;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Options;
using OpenTelemetry.Trace;

namespace Horde.Server.Agents.Fleet
{
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
		public double ScaleOutFactor { get; set; } = 0.25;

		/// <summary>
		/// Factor by which to shrink the pool size with when queue is empty
		/// The result is always rounded up to nearest integer.
		/// Example: when the queue size is zero, a default value of 0.9 will shrink the pool by 10% (current agent count * 0.9)
		/// </summary>
		public double ScaleInFactor { get; set; } = 0.9;

		/// <summary>
		/// How far back in time to look for job batches (that potentially are in the queue)
		/// </summary>
		public int SamplePeriodMin { get; set; } = 60 * 24 * 1; // 1 day

		/// <summary>
		/// Time spent in ready state before considered truly waiting for an agent
		///
		/// A job batch can be in ready state before getting picked up and executed.
		/// This threshold will help ensure only batches that have been waiting longer than this value will be considered.
		/// </summary>
		public int ReadyTimeThresholdSec { get; set; } = 45;

		/// <summary>
		/// Constructor used for JSON serialization
		/// </summary>
		[JsonConstructor]
		public JobQueueSettings()
		{
		}

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

		/// <inheritdoc />
		public override string ToString()
		{
			StringBuilder sb = new(50);
			sb.AppendFormat("{0}={1} ", nameof(ScaleOutFactor), ScaleOutFactor);
			sb.AppendFormat("{0}={1} ", nameof(ScaleInFactor), ScaleInFactor);
			sb.AppendFormat("{0}={1} ", nameof(SamplePeriodMin), SamplePeriodMin);
			sb.AppendFormat("{0}={1} ", nameof(ReadyTimeThresholdSec), ReadyTimeThresholdSec);
			return sb.ToString();
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
		private const string CacheKey = nameof(JobQueueStrategy);
		internal JobQueueSettings Settings { get; }

		private readonly IJobCollection _jobs;
		private readonly IGraphCollection _graphs;
		private readonly IStreamCollection _streamCollection;
		private readonly IClock _clock;
		private readonly IMemoryCache _cache;
		private readonly bool _isDowntimeActive;
		private readonly IOptionsMonitor<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="jobs"></param>
		/// <param name="graphs"></param>
		/// <param name="streamCollection"></param>
		/// <param name="clock"></param>
		/// <param name="cache"></param>
		/// <param name="isDowntimeActive"></param>
		/// <param name="globalConfig"></param>
		/// <param name="settings"></param>
		public JobQueueStrategy(IJobCollection jobs, IGraphCollection graphs, IStreamCollection streamCollection, IClock clock, IMemoryCache cache, bool isDowntimeActive, IOptionsMonitor<GlobalConfig> globalConfig, JobQueueSettings? settings = null)
		{
			_jobs = jobs;
			_graphs = graphs;
			_streamCollection = streamCollection;
			_clock = clock;
			_cache = cache;
			_isDowntimeActive = isDowntimeActive;
			_globalConfig = globalConfig;
			Settings = settings ?? new JobQueueSettings();
		}

		/// <inheritdoc/>
		public string Name { get; } = "JobQueue";

		/// <summary>
		/// Extract all job step batches from a job, with their associated pool 
		/// </summary>
		/// <param name="job">Job to extract from</param>
		/// <param name="streams">Cached lookup table of streams</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		private async Task<List<(IJob Job, IJobStepBatch Batch, PoolId PoolId)>> GetJobBatchesWithPoolsAsync(IJob job, Dictionary<StreamId, StreamConfig> streams, CancellationToken cancellationToken)
		{
			IGraph graph = await _graphs.GetAsync(job.GraphHash, cancellationToken);

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
				if (waitTime.Value < TimeSpan.FromSeconds(Settings.ReadyTimeThresholdSec))
				{
					continue;
				}

				if (!streams.TryGetValue(job.StreamId, out StreamConfig? streamConfig))
				{
					continue;
				}

				string batchAgentType = graph.Groups[batch.GroupIdx].AgentType;
				if (!streamConfig.AgentTypes.TryGetValue(batchAgentType, out AgentConfig? agentType))
				{
					continue;
				}

				jobBatches.Add((job, batch, agentType.Pool));
			}

			return jobBatches;
		}

		internal async Task<Dictionary<PoolId, int>> GetPoolQueueSizesAsync(DateTimeOffset jobsCreatedAfter, CancellationToken cancellationToken = default)
		{
			using TelemetrySpan span = OpenTelemetryTracers.Horde.StartActiveSpan($"{nameof(JobQueueStrategy)}.{nameof(GetPoolQueueSizesAsync)}");
			span.SetAttribute("after", jobsCreatedAfter);

			Dictionary<StreamId, StreamConfig> streams = _globalConfig.CurrentValue.Streams.ToDictionary(x => x.Id, x => (StreamConfig)x);
			IReadOnlyList<IJob> recentJobs = await _jobs.FindAsync(minCreateTime: jobsCreatedAfter, batchState: JobStepBatchState.Ready, cancellationToken: cancellationToken);
			span.SetAttribute("numJobs", recentJobs.Count);
			span.SetAttribute("numUniqueGraphs", recentJobs.Select(x => x.GraphHash).Distinct().Count());

			List<(IJob Job, IJobStepBatch Batch, PoolId PoolId)> jobBatches = new();
			foreach (IJob job in recentJobs)
			{
				jobBatches.AddRange(await GetJobBatchesWithPoolsAsync(job, streams, cancellationToken));
			}

			List<(PoolId PoolId, int QueueSize)> poolsWithQueueSize = jobBatches.GroupBy(t => t.PoolId).Select(t => (t.Key, t.Count())).ToList();

			span.SetAttribute("numPools", poolsWithQueueSize.Count);

			if (_isDowntimeActive)
			{
				// As an optimization, assume queue size is zero during maintenance windows.
				return poolsWithQueueSize.ToDictionary(x => x.PoolId, x => 0);
			}

			return poolsWithQueueSize.ToDictionary(x => x.PoolId, x => x.QueueSize);
		}

		/// <inheritdoc/>
		public async Task<PoolSizeResult> CalculatePoolSizeAsync(IPoolConfig pool, List<IAgent> agents, CancellationToken cancellationToken = default)
		{
			using TelemetrySpan span = OpenTelemetryTracers.Horde.StartActiveSpan($"{nameof(JobQueueStrategy)}.{nameof(CalculatePoolSizeAsync)}");
			span.SetAttribute(OpenTelemetryTracers.DatadogResourceAttribute, pool.Id.ToString());
			span.SetAttribute("currentAgentCount", agents.Count);
			span.SetAttribute("samplePeriodMin", Settings.SamplePeriodMin);

			DateTimeOffset minCreateTime = _clock.UtcNow - TimeSpan.FromMinutes(Settings.SamplePeriodMin);

			// Cache pool queue sizes for a short while for faster runs when many pools are scaled
			if (!_cache.TryGetValue(CacheKey, out Dictionary<PoolId, int>? poolQueueSizes) || poolQueueSizes == null)
			{
				// Pool sizes haven't been cached, update them (might happen from multiple tasks but that is fine)
				poolQueueSizes = await GetPoolQueueSizesAsync(minCreateTime, cancellationToken);
				_cache.Set(CacheKey, poolQueueSizes, TimeSpan.FromSeconds(60));
			}

			poolQueueSizes.TryGetValue(pool.Id, out int queueSize);

			Dictionary<string, object> status = new()
			{
				["Name"] = GetType().Name,
				["QueueSize"] = queueSize,
				["ScaleOutFactor"] = Settings.ScaleOutFactor,
				["ScaleInFactor"] = Settings.ScaleInFactor,
				["SamplePeriodMin"] = Settings.SamplePeriodMin,
				["ReadyTimeThresholdSec"] = Settings.ReadyTimeThresholdSec,
			};

			if (queueSize > 0)
			{
				int additionalAgentCount = (int)Math.Ceiling(queueSize * Settings.ScaleOutFactor);
				int desiredAgentCount = agents.Count + additionalAgentCount;
				return new PoolSizeResult(agents.Count, desiredAgentCount, status);
			}
			else
			{
				int desiredAgentCount = (int)(agents.Count * Settings.ScaleInFactor);
				return new PoolSizeResult(agents.Count, desiredAgentCount, status);
			}
		}
	}
}
