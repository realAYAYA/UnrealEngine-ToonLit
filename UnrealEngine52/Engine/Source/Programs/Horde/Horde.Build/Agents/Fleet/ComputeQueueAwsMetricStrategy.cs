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
using Horde.Build.Compute.V1;
using Microsoft.Extensions.Logging;
using OpenTracing;
using OpenTracing.Util;

namespace Horde.Build.Agents.Fleet
{
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
		private readonly ComputeQueueAwsMetricSettings _settings;
		private readonly ILogger<ComputeQueueAwsMetricStrategy> _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="cloudWatch"></param>
		/// <param name="computeService"></param>
		/// <param name="settings"></param>
		/// <param name="logger"></param>
		public ComputeQueueAwsMetricStrategy(IAmazonCloudWatch cloudWatch, IComputeService computeService,
			ComputeQueueAwsMetricSettings settings, ILogger<ComputeQueueAwsMetricStrategy> logger)
		{
			_cloudWatch = cloudWatch;
			_computeService = computeService;
			_settings = settings;
			_logger = logger;
		}

		/// <inheritdoc/>
		public string Name { get; } = "ComputeQueueAwsMetric";

		/// <inheritdoc/>
		public async Task<PoolSizeResult> CalculatePoolSizeAsync(IPool pool, List<IAgent> agents)
		{
			using IScope scope = GlobalTracer.Instance.BuildSpan("ComputeQueueAwsMetricStrategy.CalcDesiredPoolSizesAsync").StartActive();

			Dictionary<string, List<MetricDatum>> metricsPerCloudWatchNamespace = new();
			int numAgents = agents.Count;
			int totalCpuCores = agents.Select(x => x.Resources.TryGetValue(KnownPropertyNames.LogicalCores, out int numCpuCores) ? numCpuCores : 0).Sum();
			int numQueuedComputeTasks =	await _computeService.GetNumQueuedTasksForPoolAsync(new ClusterId(_settings.ComputeClusterId), pool);

			double numQueuedTasksPerAgent = numQueuedComputeTasks / (double)Math.Max(numAgents, 1);
			double numQueuedTasksPerCores = numQueuedComputeTasks / (double)Math.Max(totalCpuCores, 1);

			DateTime now = DateTime.UtcNow;

			if (!metricsPerCloudWatchNamespace.TryGetValue(_settings.Namespace, out List<MetricDatum>? metricDatums))
			{
				metricDatums = new List<MetricDatum>();
				metricsPerCloudWatchNamespace[_settings.Namespace] = metricDatums;
			}

			List<Dimension> dimensions = new()
			{
				new() { Name = "Pool", Value = pool.Id.ToString() },
				new() { Name = "ComputeCluster", Value = _settings.ComputeClusterId },
			};
			metricDatums.Add(new()
			{
				MetricName = "NumComputeTasksPerAgent",
				Dimensions = dimensions,
				Unit = StandardUnit.Count,
				Value = numQueuedTasksPerAgent,
				TimestampUtc = now
			});
			metricDatums.Add(new()
			{
				MetricName = "NumComputeTasksPerCpuCores",
				Dimensions = dimensions,
				Unit = StandardUnit.Count,
				Value = numQueuedTasksPerCores,
				TimestampUtc = now
			});

			foreach ((string ns, List<MetricDatum> metricDatumsNs) in metricsPerCloudWatchNamespace)
			{
				using IScope cwScope = GlobalTracer.Instance.BuildSpan("Putting CloudWatch metrics").StartActive();
				cwScope.Span.SetTag("namespace", ns);

				PutMetricDataRequest request = new() { Namespace = ns, MetricData = metricDatumsNs };
				PutMetricDataResponse response = await _cloudWatch.PutMetricDataAsync(request);
				scope.Span.SetTag("res.statusCode", (int)response.HttpStatusCode);
				if (response.HttpStatusCode != HttpStatusCode.OK)
				{
					_logger.LogError("Unable to put CloudWatch metrics");
				}
			}

			// Return the input pool size data as-is since we are only observing and not modifying
			return new PoolSizeResult(numAgents, numAgents);
		}
	}
}
