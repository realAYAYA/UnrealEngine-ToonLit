// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using StatsdClient;

namespace Horde.Build.Server
{
	/// <summary>
	/// Periodically send metrics for the CLR and other services that cannot be collected on a per-request basis
	/// </summary>
	public sealed class MetricService : IHostedService, IDisposable
	{
		readonly IDogStatsd _dogStatsd;
		readonly ITicker _ticker;

		/// <summary>
		/// Constructor
		/// </summary>
		public MetricService(IDogStatsd dogStatsd, IClock clock, ILogger<MetricService> logger)
		{
			_dogStatsd = dogStatsd;
			_ticker = clock.AddTicker<MetricService>(TimeSpan.FromSeconds(20.0), TickAsync, logger);
		}

		/// <inheritdoc/>
		public Task StartAsync(CancellationToken cancellationToken) => _ticker.StartAsync();

		/// <inheritdoc/>
		public Task StopAsync(CancellationToken cancellationToken) => _ticker.StopAsync();

		/// <inheritdoc/>
		public void Dispose() => _ticker.Dispose();

		/// <summary>
		/// Execute the background task
		/// </summary>
		/// <param name="stoppingToken">Cancellation token for the async task</param>
		/// <returns>Async task</returns>
		ValueTask TickAsync(CancellationToken stoppingToken)
		{
			ReportGcMetrics();
			ReportThreadMetrics();
			return new ValueTask();
		}

		private void ReportThreadMetrics()
		{
			ThreadPool.GetMaxThreads(out int maxWorkerThreads, out int maxIoThreads);
			ThreadPool.GetAvailableThreads(out int freeWorkerThreads, out int freeIoThreads);
			ThreadPool.GetMinThreads(out int minWorkerThreads, out int minIoThreads);

			int busyIoThreads = maxIoThreads - freeIoThreads;
			int busyWorkerThreads = maxWorkerThreads - freeWorkerThreads;

			_dogStatsd.Gauge("horde.clr.threadpool.io.max", maxIoThreads);
			_dogStatsd.Gauge("horde.clr.threadpool.io.min", minIoThreads);
			_dogStatsd.Gauge("horde.clr.threadpool.io.free", freeIoThreads);
			_dogStatsd.Gauge("horde.clr.threadpool.io.busy", busyIoThreads);

			_dogStatsd.Gauge("horde.clr.threadpool.worker.max", maxWorkerThreads);
			_dogStatsd.Gauge("horde.clr.threadpool.worker.min", minWorkerThreads);
			_dogStatsd.Gauge("horde.clr.threadpool.worker.free", freeWorkerThreads);
			_dogStatsd.Gauge("horde.clr.threadpool.worker.busy", busyWorkerThreads);
		}

		private void ReportGcMetrics()
		{
			GCMemoryInfo gcMemoryInfo = GC.GetGCMemoryInfo();

			_dogStatsd.Gauge("horde.clr.gc.totalMemory", GC.GetTotalMemory(false));
			_dogStatsd.Gauge("horde.clr.gc.totalAllocated", GC.GetTotalAllocatedBytes());
			_dogStatsd.Gauge("horde.clr.gc.heapSize", gcMemoryInfo.HeapSizeBytes);
			_dogStatsd.Gauge("horde.clr.gc.fragmented", gcMemoryInfo.FragmentedBytes);
			_dogStatsd.Gauge("horde.clr.gc.memoryLoad", gcMemoryInfo.MemoryLoadBytes);
			_dogStatsd.Gauge("horde.clr.gc.totalAvailableMemory", gcMemoryInfo.TotalAvailableMemoryBytes);
			_dogStatsd.Gauge("horde.clr.gc.highMemoryLoadThreshold", gcMemoryInfo.HighMemoryLoadThresholdBytes);
		}
	}
}
