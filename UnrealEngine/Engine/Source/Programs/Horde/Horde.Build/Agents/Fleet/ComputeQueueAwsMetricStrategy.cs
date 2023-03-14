// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Threading.Tasks;
using Amazon.CloudWatch;
using Amazon.CloudWatch.Model;
using EpicGames.Horde.Compute;
using Horde.Build.Agents.Pools;
using Horde.Build.Compute;
using Horde.Build.Utilities;
using Microsoft.Extensions.Logging;
using OpenTracing;
using OpenTracing.Util;

namespace Horde.Build.Agents.Fleet
{
	using PoolId = StringId<IPool>;

	/// <summary>
	/// Settings for <see cref="ComputeQueueAwsMetricStrategy" />
	/// </summary>
	public class ComputeQueueAwsMetricSettings
	{
		/// <summary>
		/// Compute cluster ID to observe
		/// </summary>
		public string ComputeClusterId { get; set; }
		
		/// <summary>
		/// AWS CloudWatch namespace to write metrics in
		/// </summary>
		public string Namespace { get; set; }
		
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="computeClusterId"></param>
		/// <param name="cloudWatchNamespace"></param>
		public ComputeQueueAwsMetricSettings(string computeClusterId, string cloudWatchNamespace = "HordeBuild")
		{
			ComputeClusterId = computeClusterId;
			Namespace = cloudWatchNamespace;
		}
	}
	
	/// <summary>
	/// A no-op strategy that reports size of compute task queue for a given pool as AWS CloudWatch metrics
	/// A metric that later can be used as a source for AWS-controlled auto-scaling policies. 
	/// </summary>
	public class ComputeQueueAwsMetricStrategy : IPoolSizeStrategy
	{
		private readonly IAmazonCloudWatch _cloudWatch;
		private readonly IComputeService _computeService;
		private readonly ILogger<ComputeQueueAwsMetricStrategy> _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="cloudWatch"></param>
		/// <param name="computeService"></param>
		/// <param name="logger"></param>
		public ComputeQueueAwsMetricStrategy(IAmazonCloudWatch cloudWatch, IComputeService computeService, ILogger<ComputeQueueAwsMetricStrategy> logger)
		{
			_cloudWatch = cloudWatch;
			_computeService = computeService;
			_logger = logger;
		}

		/// <inheritdoc/>
		public string Name { get; } = "ComputeQueueAwsMetric";

		/// <inheritdoc/>
		public async Task<List<PoolSizeData>> CalcDesiredPoolSizesAsync(List<PoolSizeData> pools)
		{
			using IScope scope = GlobalTracer.Instance.BuildSpan("ComputeQueueAwsMetricStrategy.CalcDesiredPoolSizesAsync").StartActive();

			Dictionary<string, List<MetricDatum>> metricsPerCloudWatchNamespace = new();
			foreach (PoolSizeData poolSizeData in pools)
			{
				ComputeQueueAwsMetricSettings? settings = poolSizeData.Pool.ComputeQueueAwsMetricSettings;
				if (settings == null)
				{
					_logger.LogWarning("Pool {PoolId} is configured to use strategy {Strategy} but has no settings configured", poolSizeData.Pool.Id, nameof(ComputeQueueAwsMetricStrategy));
					continue;
				}
				
				int numAgents = poolSizeData.Agents.Count;
				int totalCpuCores = poolSizeData.Agents.Select(x => x.Resources.TryGetValue(KnownPropertyNames.LogicalCores, out int numCpuCores) ? numCpuCores : 0).Sum();
				int numQueuedComputeTasks = await _computeService.GetNumQueuedTasksForPoolAsync(new ClusterId(settings.ComputeClusterId), poolSizeData.Pool);
				
				double numQueuedTasksPerAgent = numQueuedComputeTasks / (double)Math.Max(numAgents, 1);
				double numQueuedTasksPerCores = numQueuedComputeTasks / (double)Math.Max(totalCpuCores, 1);

				DateTime now = DateTime.UtcNow;
				
				if (!metricsPerCloudWatchNamespace.TryGetValue(settings.Namespace, out List<MetricDatum>? metricDatums))
				{
					metricDatums = new List<MetricDatum>();
					metricsPerCloudWatchNamespace[settings.Namespace] = metricDatums;
				}
				
				List<Dimension> dimensions = new ()
				{
					new () { Name = "Pool", Value = poolSizeData.Pool.Id.ToString() },
					new () { Name = "ComputeCluster", Value = settings.ComputeClusterId },
				};
				metricDatums.Add(new ()
				{
					MetricName = "NumComputeTasksPerAgent",
					Dimensions = dimensions,
					Unit = StandardUnit.Count,
					Value = numQueuedTasksPerAgent,
					TimestampUtc = now
				});
				metricDatums.Add(new ()
				{
					MetricName = "NumComputeTasksPerCpuCores",
					Dimensions = dimensions,
					Unit = StandardUnit.Count,
					Value = numQueuedTasksPerCores,
					TimestampUtc = now
				});
			}

			foreach ((string ns, List<MetricDatum> metricDatums) in metricsPerCloudWatchNamespace)
			{
				using IScope cwScope = GlobalTracer.Instance.BuildSpan("Putting CloudWatch metrics").StartActive();
				cwScope.Span.SetTag("namespace", ns);
				
				PutMetricDataRequest request = new() { Namespace = ns, MetricData = metricDatums };
				PutMetricDataResponse response = await _cloudWatch.PutMetricDataAsync(request);
				scope.Span.SetTag("res.statusCode", (int)response.HttpStatusCode);
				if (response.HttpStatusCode != HttpStatusCode.OK)
				{
					_logger.LogError("Unable to put CloudWatch metrics");
				}
			}

			// Return the input pool size data as-is since we are only observing and not modifying
			return pools;
		}
	}
}
