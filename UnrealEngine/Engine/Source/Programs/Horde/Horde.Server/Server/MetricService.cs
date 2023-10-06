// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics.Metrics;
using System.Threading;
using System.Threading.Tasks;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Server
{
	/// <summary>
	/// Periodically send metrics for the CLR and other services that cannot be collected on a per-request basis
	/// </summary>
	public sealed class MetricService : IHostedService, IDisposable
	{
		readonly Meter _meter;
		readonly ITicker _ticker;
		
		private readonly Gauge<int> _threadPoolIoMax;
		private readonly Gauge<int> _threadPoolIoMin;
		private readonly Gauge<int> _threadPoolIoFree;
		private readonly Gauge<int> _threadPoolIoBusy;
		private readonly Gauge<int> _threadPoolWorkerMax;
		private readonly Gauge<int> _threadPoolWorkerMin;
		private readonly Gauge<int> _threadPoolWorkerFree;
		private readonly Gauge<int> _threadPoolWorkerBusy;
		private readonly Gauge<long> _clrGcTotalMem;
		private readonly Gauge<long> _clrGcTotalAllocated;
		private readonly Gauge<long> _clrGcHeapSize;
		private readonly Gauge<long> _clrGcFragmented;
		private readonly Gauge<long> _clrGcMemoryLoad;
		private readonly Gauge<long> _clrGcTotalAvailableMem;
		private readonly Gauge<long> _clrGcHighMemThreshold;

		/// <summary>
		/// Constructor
		/// </summary>
		public MetricService(Meter meter, IClock clock, ILogger<MetricService> logger)
		{
			_meter = meter;
			_ticker = clock.AddTicker<MetricService>(TimeSpan.FromSeconds(20.0), TickAsync, logger);

			_threadPoolIoMax = _meter.CreateGauge<int>("horde.clr.threadpool.io.max");
			_threadPoolIoMin = _meter.CreateGauge<int>("horde.clr.threadpool.io.min");
			_threadPoolIoFree = _meter.CreateGauge<int>("horde.clr.threadpool.io.free");
			_threadPoolIoBusy = _meter.CreateGauge<int>("horde.clr.threadpool.io.busy");
			_threadPoolWorkerMax = _meter.CreateGauge<int>("horde.clr.threadpool.worker.max");
			_threadPoolWorkerMin = _meter.CreateGauge<int>("horde.clr.threadpool.worker.min");
			_threadPoolWorkerFree = _meter.CreateGauge<int>("horde.clr.threadpool.worker.free");
			_threadPoolWorkerBusy = _meter.CreateGauge<int>("horde.clr.threadpool.worker.busy");
			_clrGcTotalMem = _meter.CreateGauge<long>("horde.clr.gc.totalMemory");
			_clrGcTotalAllocated = _meter.CreateGauge<long>("horde.clr.gc.totalAllocated");
			_clrGcHeapSize = _meter.CreateGauge<long>("horde.clr.gc.heapSize");
			_clrGcFragmented = _meter.CreateGauge<long>("horde.clr.gc.fragmented");
			_clrGcMemoryLoad = _meter.CreateGauge<long>("horde.clr.gc.memoryLoad");
			_clrGcTotalAvailableMem = _meter.CreateGauge<long>("horde.clr.gc.totalAvailableMemory");
			_clrGcHighMemThreshold = _meter.CreateGauge<long>("horde.clr.gc.highMemoryLoadThreshold");
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
			
			_threadPoolIoMax.Record(maxIoThreads);
			_threadPoolIoMin.Record(minIoThreads);
			_threadPoolIoFree.Record(freeIoThreads);
			_threadPoolIoBusy.Record(busyIoThreads);
			_threadPoolWorkerMax.Record(maxWorkerThreads);
			_threadPoolWorkerMin.Record(minWorkerThreads);
			_threadPoolWorkerFree.Record(freeWorkerThreads);
			_threadPoolWorkerBusy.Record(busyWorkerThreads);
		}

		private void ReportGcMetrics()
		{
			GCMemoryInfo gcMemoryInfo = GC.GetGCMemoryInfo();

			_clrGcTotalMem.Record(GC.GetTotalMemory(false));
			_clrGcTotalAllocated.Record(GC.GetTotalAllocatedBytes());
			_clrGcHeapSize.Record(gcMemoryInfo.HeapSizeBytes);
			_clrGcFragmented.Record(gcMemoryInfo.FragmentedBytes);
			_clrGcMemoryLoad.Record(gcMemoryInfo.MemoryLoadBytes);
			_clrGcTotalAvailableMem.Record(gcMemoryInfo.TotalAvailableMemoryBytes);
			_clrGcHighMemThreshold.Record(gcMemoryInfo.HighMemoryLoadThresholdBytes);
		}
	}
}
