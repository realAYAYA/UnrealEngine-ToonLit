// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using EpicGames.Core;
using Google.Protobuf.WellKnownTypes;
using Grpc.Core;
using Grpc.Net.Client;
using Horde.Agent.Leases.Handlers;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Messages.Telemetry;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using Microsoft.Management.Infrastructure;

namespace Horde.Agent.Services;

/// <summary>
/// Metrics for CPU usage
/// </summary>
public class CpuMetrics
{
	/// <summary>
	/// Percentage of time the CPU was busy executing code in user space
	/// </summary>
	public float User { get; set; }

	/// <summary>
	/// Percentage of time the CPU was busy executing code in kernel space
	/// </summary>
	public float System { get; set; }

	/// <summary>
	/// Percentage of time the CPU was idling
	/// </summary>
	public float Idle { get; set; }

	/// <inheritdoc />
	public override string ToString()
	{
		return $"User={User,5:F1}% System={System,5:F1}% Idle={Idle,5:F1}%";
	}

	/// <summary>
	/// Convert to Protobuf-based event
	/// </summary>
	/// <returns></returns>
	public AgentCpuMetricsEvent ToEvent()
	{
		return new AgentCpuMetricsEvent { User = User, System = System, Idle = Idle };
	}
}

/// <summary>
/// Metrics for memory usage
/// </summary>
public class MemoryMetrics
{
	/// <summary>
	/// Total memory installed (kibibytes)
	/// </summary>
	public uint Total { get; set; }

	/// <summary>
	/// Available memory (kibibytes)
	/// </summary>
	public uint Available { get; set; }

	/// <summary>
	/// Used memory (kibibytes)
	/// </summary>
	public uint Used { get; set; }

	/// <summary>
	/// Used memory (percentage)
	/// </summary>
	public float UsedPercentage { get; set; }

	/// <inheritdoc />
	public override string ToString()
	{
		return $"Total={Total} kB, Available={Available} kB, Used={Used} kB, Used={UsedPercentage * 100.0:F1} %";
	}

	/// <summary>
	/// Convert to Protobuf-based event
	/// </summary>
	/// <returns></returns>
	public AgentMemoryMetricsEvent ToEvent()
	{
		return new AgentMemoryMetricsEvent { Total = Total, Free = Available, Used = Used, UsedPercentage = UsedPercentage };
	}
}

/// <summary>
/// OS agnostic interface for retrieving system metrics (CPU, memory etc)
/// </summary>
public interface ISystemMetrics : IDisposable
{
	/// <summary>
	/// Get CPU usage metrics
	/// </summary>
	/// <returns>An object with CPU usage metrics</returns>
	CpuMetrics GetCpu();

	/// <summary>
	/// Get memory usage metrics
	/// </summary>
	/// <returns>An object with memory usage metrics</returns>
	MemoryMetrics GetMemory();
}

// Suppress call sites not available on all platforms
#pragma warning disable CA1416

/// <summary>
/// Windows specific implementation for gathering system metrics
/// </summary>
public sealed class WindowsSystemMetrics : ISystemMetrics
{
	private const string ProcessorInfo = "Processor Information"; // Prefer this over "Processor" as it's more modern
	private const string Memory = "Memory";
	private const string Total = "_Total";

	private readonly PerformanceCounter _procIdleTime = new(ProcessorInfo, "% Idle Time", Total);
	private readonly PerformanceCounter _procUserTime = new(ProcessorInfo, "% User Time", Total);
	private readonly PerformanceCounter _procPrivilegedTime = new(ProcessorInfo, "% Privileged Time", Total);

	private readonly uint _totalPhysicalMemory = GetPhysicalMemory();
	private readonly PerformanceCounter _memAvailableBytes = new(Memory, "Available Bytes");

	/// <summary>
	/// Constructor
	/// </summary>
	public WindowsSystemMetrics()
	{
		GetCpu(); // Trigger this to ensure performance counter has a fetched value. Avoids an initial zero result when called later.
	}

	/// <inheritdoc/>
	public void Dispose()
	{
		_memAvailableBytes.Dispose();
		_procIdleTime.Dispose();
		_procUserTime.Dispose();
		_procPrivilegedTime.Dispose();
	}

	/// <inheritdoc />
	public CpuMetrics GetCpu()
	{
		return new CpuMetrics
		{
			User = _procUserTime.NextValue(),
			System = _procPrivilegedTime.NextValue(),
			Idle = _procIdleTime.NextValue()
		};
	}

	/// <inheritdoc />
	public MemoryMetrics GetMemory()
	{
		uint available = (uint)(_memAvailableBytes.NextValue() / 1024);
		uint used = _totalPhysicalMemory - available;
		return new MemoryMetrics
		{
			Total = _totalPhysicalMemory,
			Available = available,
			Used = used,
			UsedPercentage = used / (float)_totalPhysicalMemory,
		};
	}

	private static uint GetPhysicalMemory()
	{
		using CimSession session = CimSession.Create(null);
		const string QueryNamespace = @"root\cimv2";
		const string QueryDialect = "WQL";
		ulong totalCapacity = 0;

		foreach (CimInstance instance in session.QueryInstances(QueryNamespace, QueryDialect, "select Capacity from Win32_PhysicalMemory"))
		{
			foreach (CimProperty property in instance.CimInstanceProperties)
			{
				if (property.Name.Equals("Capacity", StringComparison.OrdinalIgnoreCase) && property.Value is ulong capacity)
				{
					totalCapacity += capacity;
				}
			}
		}

		return (uint)(totalCapacity / 1024); // as kibibytes
	}
}

#pragma warning restore CA1416

/// <summary>
/// Send telemetry events back to server at regular intervals
/// </summary>
class TelemetryService : BackgroundService
{
	private readonly TimeSpan _heartbeatInterval = TimeSpan.FromSeconds(60);
	private readonly TimeSpan _heartbeatMaxAllowedDiff = TimeSpan.FromSeconds(5);

	private readonly WorkerService _workerService;
	private readonly JobHandler _jobHandler;
	private readonly GrpcService _grpcService;
	private readonly AgentSettings _agentSettings;
	private readonly ILogger<TelemetryService> _logger;
	private readonly TimeSpan _reportInterval;
	private readonly AgentMetadataEvent _agentMetadataEvent;
	private readonly TimeSpan _agentMetadataReportInterval = TimeSpan.FromMinutes(2);
	private ISystemMetrics? _systemMetrics;
	private DateTime _lastTimeAgentMetadataSent = DateTime.UnixEpoch;

	private CancellationTokenSource? _eventLoopHeartbeatCts;
	private Task? _eventLoopTask;
	internal Func<DateTime> GetUtcNow { get; set; } = () => DateTime.UtcNow;

	/// <summary>
	/// Constructor
	/// </summary>
	public TelemetryService(WorkerService workerService, JobHandler jobHandler, GrpcService grpcService, IOptions<AgentSettings> settings, ILogger<TelemetryService> logger)
	{
		_workerService = workerService;
		_jobHandler = jobHandler;
		_grpcService = grpcService;
		_agentSettings = settings.Value;
		_logger = logger;
		_reportInterval = TimeSpan.FromMilliseconds(_agentSettings.TelemetryReportInterval);

		// Calculate this once at startup as it should not change during lifetime of process
		_agentMetadataEvent = GetAgentMetadataEvent();
	}

	/// <inheritdoc/>
	public override void Dispose()
	{
		base.Dispose();
		_systemMetrics?.Dispose();
		_eventLoopHeartbeatCts?.Dispose();
	}

	/// <inheritdoc />
	public override Task StartAsync(CancellationToken cancellationToken)
	{
		if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
		{
			try
			{
				_systemMetrics ??= new WindowsSystemMetrics();
			}
			catch (Exception e)
			{
				_logger.LogError(e, "Unable to initialize system metric collector for telemetry. Disabling. Reason: {Message}", e.Message);
			}
		}
		else
		{
			_logger.LogInformation("System metric collection only implemented on Windows");
		}

		_eventLoopHeartbeatCts = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
		_eventLoopTask = EventLoopHeartbeatAsync(_eventLoopHeartbeatCts.Token);

		return base.StartAsync(cancellationToken);
	}

	public override async Task StopAsync(CancellationToken cancellationToken)
	{
		_eventLoopHeartbeatCts?.Cancel();

		if (_eventLoopTask != null)
		{
			try
			{
				await _eventLoopTask;
			}
			catch (OperationCanceledException)
			{
				// Ignore cancellation exceptions
			}
		}

		await base.StopAsync(cancellationToken);
	}

	/// <summary>
	/// Continuously run the heartbeat for the event loop
	/// </summary>
	/// <param name="cancellationToken">Cancellation token</param>
	private async Task EventLoopHeartbeatAsync(CancellationToken cancellationToken)
	{
		while (!cancellationToken.IsCancellationRequested)
		{
			(bool onTime, TimeSpan diff) = await IsEventLoopOnTimeAsync(_heartbeatInterval, _heartbeatMaxAllowedDiff, cancellationToken);
			if (!onTime)
			{
				_logger.LogWarning("Event loop heartbeat was not on time. Diff {Diff} ms", diff.TotalMilliseconds);
			}
		}
	}

	/// <summary>
	/// Checks if the async event loop is on time
	/// </summary>
	/// <param name="wait">Time to wait</param>
	/// <param name="maxDiff">Max allowed diff</param>
	/// <param name="cancellationToken">Cancellation token</param>
	/// <returns>A tuple with the result</returns>
	public async Task<(bool onTime, TimeSpan diff)> IsEventLoopOnTimeAsync(TimeSpan wait, TimeSpan maxDiff, CancellationToken cancellationToken)
	{
		try
		{
			DateTime before = GetUtcNow();
			await Task.Delay(wait, cancellationToken);
			DateTime after = GetUtcNow();
			TimeSpan diff = after - before - wait;
			return (diff.Duration() < maxDiff, diff);
		}
		catch (TaskCanceledException)
		{
			// Ignore
			return (true, TimeSpan.Zero);
		}
	}

	/// <summary>
	/// Get list of any filter drivers known to be problematic for builds
	/// </summary>
	/// <param name="logger">Logger to use</param>
	/// <param name="cancellationToken">Cancellation token for the call</param>
	public static async Task<List<string>> GetProblematicFilterDriversAsync(ILogger logger, CancellationToken cancellationToken)
	{
		try
		{
			List<string> problematicDrivers = new()
			{
				// PlayStation SDK related filter drivers known to have negative effects on file system performance
				"cbfilter",
				"cbfsfilter",
				"cbfsconnect",
				"sie-filemon",

				"csagent", // CrowdStrike
			};

			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				string output = await ReadFltMcOutputAsync(cancellationToken);
				List<string>? drivers = ParseFltMcOutput(output);
				if (drivers == null)
				{
					logger.LogWarning("Unable to get loaded filter drivers");
					return new List<string>();
				}

				List<string> loadedDrivers = drivers
					.Where(x =>
					{
						foreach (string probDriverName in problematicDrivers)
						{
							if (x.Contains(probDriverName, StringComparison.OrdinalIgnoreCase))
							{
								return true;
							}
						}
						return false;
					})
					.ToList();

				return loadedDrivers;
			}
		}
		catch (Exception e)
		{
			logger.LogError(e, "Error logging filter drivers");
		}

		return new List<string>();
	}

	/// <summary>
	/// Log any filter drivers known to be problematic for builds
	/// </summary>
	/// <param name="logger">Logger to use</param>
	/// <param name="cancellationToken">Cancellation token for the call</param>
	public static async Task LogProblematicFilterDriversAsync(ILogger logger, CancellationToken cancellationToken)
	{
		List<string> loadedDrivers = await GetProblematicFilterDriversAsync(logger, cancellationToken);
		if (loadedDrivers.Count > 0)
		{
			logger.LogWarning("Agent has problematic filter drivers loaded: {FilterDrivers}", String.Join(',', loadedDrivers));
		}
	}

	internal static async Task<string> ReadFltMcOutputAsync(CancellationToken cancellationToken)
	{
		string fltmcExePath = Path.Combine(Environment.SystemDirectory, "fltmc.exe");
		using CancellationTokenSource cancellationSource = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
		cancellationSource.CancelAfter(10000);
		using ManagedProcess process = new(null, fltmcExePath, "filters", null, null, null, ProcessPriorityClass.Normal);
		StringBuilder sb = new(1000);

		while (!cancellationToken.IsCancellationRequested)
		{
			string? line = await process.ReadLineAsync(cancellationToken);
			if (line == null)
			{
				break;
			}

			sb.AppendLine(line);
		}

		await process.WaitForExitAsync(cancellationToken);
		return sb.ToString();
	}

	internal static List<string>? ParseFltMcOutput(string output)
	{
		if (output.Contains("access is denied", StringComparison.OrdinalIgnoreCase))
		{
			return null;
		}

		List<string> filters = new();
		string[] lines = output.Split(new[] { "\r\n", "\r", "\n" }, StringSplitOptions.None);

		foreach (string line in lines)
		{
			if (line.Length < 5)
			{
				continue;
			}
			if (line.StartsWith("---", StringComparison.Ordinal))
			{
				continue;
			}
			if (line.StartsWith("Filter", StringComparison.Ordinal))
			{
				continue;
			}

			string[] parts = line.Split("   ", StringSplitOptions.RemoveEmptyEntries);
			string filterName = parts[0];
			filters.Add(filterName);
		}

		return filters;
	}

	/// <inheritdoc />
	protected override async Task ExecuteAsync(CancellationToken stoppingToken)
	{
		while (!stoppingToken.IsCancellationRequested)
		{
			try
			{
				if (!await ExecuteInternalAsync(stoppingToken))
				{
					break;
				}
			}
			catch (OperationCanceledException) when (stoppingToken.IsCancellationRequested)
			{
				break;
			}
			catch (Exception ex)
			{
				_logger.LogWarning(ex, "Exception in TelemetryService: {Message}", ex.Message);
			}

			// Wait a moment before attempting to restart the background work
			await Task.Delay(TimeSpan.FromSeconds(20), stoppingToken);
		}
	}

	private async Task<bool> ExecuteInternalAsync(CancellationToken stoppingToken)
	{
		if (_systemMetrics == null || !_agentSettings.EnableTelemetry)
		{
			return false;
		}

		using GrpcChannel channel = await _grpcService.CreateGrpcChannelAsync(stoppingToken);
		CallInvoker invoker = _grpcService.GetInvoker(channel);
		HordeRpc.HordeRpcClient client = new(invoker);

		while (!stoppingToken.IsCancellationRequested)
		{
			_logger.LogDebug("Sending telemetry events to server...");

			SendTelemetryEventsRequest request = new();
			Timestamp utcNow = Timestamp.FromDateTime(DateTime.UtcNow);
			ExecutionMetadata em = new()
			{
				LeaseId = _jobHandler.CurrentLeaseId.ToString(),
				JobId = _jobHandler.CurrentJobId,
				JobBatchId = _jobHandler.CurrentBatchId,
			};

			{
				AgentCpuMetricsEvent cpuMetricsEvent = _systemMetrics.GetCpu().ToEvent();
				cpuMetricsEvent.AgentId = _agentMetadataEvent.AgentId;
				cpuMetricsEvent.Timestamp = utcNow;
				cpuMetricsEvent.ExecutionMetadata = em;
				request.Events.Add(new WrappedTelemetryEvent { Cpu = cpuMetricsEvent });
			}

			{
				AgentMemoryMetricsEvent memMetricsEvent = _systemMetrics.GetMemory().ToEvent();
				memMetricsEvent.AgentId = _agentMetadataEvent.AgentId;
				memMetricsEvent.Timestamp = utcNow;
				memMetricsEvent.ExecutionMetadata = em;
				request.Events.Add(new WrappedTelemetryEvent { Mem = memMetricsEvent });
			}

			if (DateTime.UtcNow > _lastTimeAgentMetadataSent + _agentMetadataReportInterval)
			{
				// Report agent metadata every now and then as events are not guaranteed to be delivered.
				// Re-sending ensures the metadata will eventually make it to the server.
				request.Events.Add(new WrappedTelemetryEvent { AgentMetadata = _agentMetadataEvent });
				_lastTimeAgentMetadataSent = DateTime.UtcNow;
			}

			await client.SendTelemetryEventsAsync(request, new CallOptions(cancellationToken: stoppingToken));
			await Task.Delay(_reportInterval, stoppingToken);
		}

		return true;
	}

	AgentMetadataEvent GetAgentMetadataEvent()
	{
		AgentMetadataEvent e = new()
		{
			Ip = null,
			Hostname = _agentSettings.GetAgentName(),
			Region = null,
			AvailabilityZone = null,
			Environment = null,
			AgentVersion = AgentApp.Version,
			Os = GetOs(),
			OsVersion = Environment.OSVersion.Version.ToString(),
			Architecture = RuntimeInformation.OSArchitecture.ToString(),
		};
		e.PoolIds.AddRange(_workerService.PoolIds);
		e.AgentId = e.CalculateAgentId();
		return e;
	}

	private static string GetOs()
	{
		if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
		{
			return "Windows";
		}
		if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
		{
			return "Linux";
		}
		if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
		{
			return "macOS";
		}
		return "Unknown";
	}
}