// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Horde.Server.Jobs;
using Horde.Server.Notifications;
using Horde.Server.Server;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Server.Devices
{
#pragma warning disable CS1591 // Missing XML comment for publicly visible type or member
	public class DeviceReport
	{
		public string DeviceId { get; }
		public string DeviceName { get; }
		public string DeviceAddress { get; }

		public string PlatformId { get; }
		public string PlatformName { get; }

		public string PoolId { get; }
		public string PoolName { get; }

		/// <summary>
		/// Problems reported since the last report
		/// </summary>
		public int ProblemDelta { get; set; } = 0;

		/// <summary>
		/// The last problem Description
		/// </summary>
		public string? LastProblemDesc { get; set; }

		/// <summary>
		/// The last problem encountered
		/// </summary>
		public string? LastProblemURL { get; set; }

		public List<IDeviceTelemetry> Telemetry { get; }

		public DeviceReport(string platformId, string platformName, string deviceId, string deviceName, string deviceAddress, string poolId, string poolName, List<IDeviceTelemetry> telemetry)
		{
			PlatformId = platformId;
			PlatformName = platformName;
			DeviceId = deviceId;
			DeviceName = deviceName;
			DeviceAddress = deviceAddress;
			PoolId = poolId;
			PoolName = poolName;
			Telemetry = telemetry;
		}
	}

	public class DevicePlatformReport
	{
		public string PlatformId { get; }
		public string PlatformName { get; }
		public List<DeviceReport> DeviceReports { get; set; } = new List<DeviceReport>();

		public DevicePlatformReport(string platformId, string platformName)
		{
			PlatformId = platformId;
			PlatformName = platformName;
		}
	}

	public class DevicePoolMetrics
	{
		public string PlatformId { get; }
		public string PlatformName { get; }
		public int Total { get; set; } = 0;
		public int Disabled { get; set; } = 0;
		public int Maintenance { get; set; } = 0;
		public int Problems { get; set; } = 0;

		public DevicePoolMetrics(string platformId, string platformName)
		{
			PlatformId = platformId;
			PlatformName = platformName;
		}
	}

	public class DevicePoolReport
	{
		public string PoolId { get; }
		public string PoolName { get; }

		public List<DevicePoolMetrics> Metrics { get; } = new List<DevicePoolMetrics>();

		public DevicePoolReport(string poolId, string poolName)
		{
			PoolId = poolId;
			PoolName = poolName;
		}
	}

	public class DeviceIssueReport
	{
		public string Channel { get; }

		public List<DevicePlatformReport> PlatformReports { get; set; } = new List<DevicePlatformReport>();

		public List<DevicePoolReport> PoolReports { get; set; } = new List<DevicePoolReport>();

		public DeviceIssueReport(string channel)
		{
			Channel = channel;
		}
	}

#pragma warning restore CS1591 // Missing XML comment for publicly visible type or member

	[SingletonDocument("device-report-state", "6491bc0ce39f8b70b1ef2feb")]
	class DeviceReportState : SingletonBase
	{
		public DateTime ReportTime { get; set; } = DateTime.MinValue;
	}

	/// <summary>
	/// Posts summaries for all the open issues in different streams to Slack channels
	/// </summary>
	public class DeviceReportService : IHostedService
	{

		readonly SingletonDocument<DeviceReportState> _state;
		readonly DeviceService _deviceService;
		readonly INotificationService _notificationService;
		readonly IClock _clock;
		readonly ITicker _ticker;
		readonly IOptionsMonitor<GlobalConfig> _globalConfig;
		readonly ILogger<DeviceReportService> _logger;

		readonly int _reportIntervalMinutes = 180;

		/// <summary>
		/// Constructor
		/// </summary>
		public DeviceReportService(MongoService mongoService, DeviceService deviceService, INotificationService notificationService, IClock clock, IOptionsMonitor<GlobalConfig> globalConfig, ILogger<DeviceReportService> logger)
		{
			_state = new SingletonDocument<DeviceReportState>(mongoService);
			_deviceService = deviceService;
			_notificationService = notificationService;
			_clock = clock;
			_ticker = clock.AddSharedTicker<DeviceReportService>(TimeSpan.FromMinutes(5.0), TickAsync, logger);
			_globalConfig = globalConfig;
			_logger = logger;
		}

		/// <inheritdoc/>
		public async Task StartAsync(CancellationToken cancellationToken)
		{
			await _ticker.StartAsync();
		}

		/// <inheritdoc/>
		public async Task StopAsync(CancellationToken cancellationToken)
		{
			await _ticker.StopAsync();
		}

		async ValueTask TickAsync(CancellationToken cancellationToken)
		{
			GlobalConfig globalConfig = _globalConfig.CurrentValue;

			if (String.IsNullOrEmpty(globalConfig.ServerSettings.DeviceReportChannel))
			{
				return;
			}

			DeviceReportState state = await _state.GetAsync(cancellationToken);
			DateTime currentTime = _clock.UtcNow;
			DateTime lastReportTime = state.ReportTime == DateTime.MinValue ? DateTime.Now.Subtract(TimeSpan.FromMinutes(_reportIntervalMinutes + 1)) : state.ReportTime;

			if ((currentTime - lastReportTime).TotalMinutes <= _reportIntervalMinutes)
			{
				return;
			}

			_logger.LogInformation("Creating device report");

			List<IDevicePool> pools = _deviceService.GetPools();
			List<IDevicePlatform> platforms = _deviceService.GetPlatforms();
			List<IDevice> devices = await _deviceService.GetDevicesAsync();

			devices = devices.OrderBy(d => d.PlatformId.ToString()).ThenBy(d => d.PoolId.ToString()).ToList();

			List<IDeviceTelemetry> deviceTelemetry = await _deviceService.GetDeviceTelemetryAsync(null, lastReportTime);

			DeviceIssueReport issueReport = new DeviceIssueReport(globalConfig.ServerSettings.DeviceReportChannel);

			devices.ForEach(device =>
			{
				IDevicePool? pool = pools.Find(p => p.Id == device.PoolId);
				IDevicePlatform? platform = platforms.Find(p => p.Id == device.PlatformId);

				if (pool == null || platform == null)
				{
					return;
				}

				if (pool.PoolType != DevicePoolType.Automation)
				{
					return;
				}

				DevicePoolReport? poolReport = issueReport.PoolReports.Find(r => r.PoolId == pool.Id.ToString());
				if (poolReport == null)
				{
					poolReport = new DevicePoolReport(pool.Id.ToString(), pool.Name.ToString());
					issueReport.PoolReports.Add(poolReport);
				}

				DevicePoolMetrics? metrics = poolReport.Metrics.Find(m => m.PlatformId == device.PlatformId.ToString());
				if (metrics == null)
				{
					metrics = new DevicePoolMetrics(platform.Id.ToString(), platform.Name.ToString());
					poolReport.Metrics.Add(metrics);
				}

				metrics.Total++;
				if (!device.Enabled)
				{
					metrics.Disabled++;
				}
				else if (device.MaintenanceTimeUtc != null)
				{
					metrics.Maintenance++;
				}

				if (!device.Enabled || device.MaintenanceTimeUtc != null)
				{
					return;
				}

				// Report device issues, though only if not since disabled or put into maintenance

				List<IDeviceTelemetry> telemetry = deviceTelemetry.Where(t => t.DeviceId == device.Id).ToList();
				List<IDeviceTelemetry> problems = telemetry.Where(t => t.ProblemTimeUtc != null && t.ProblemTimeUtc! > lastReportTime).OrderByDescending(t => t.ProblemTimeUtc!).ToList();

				if (problems.Count == 0)
				{
					return;
				}

				// mark that this device reported a problem
				metrics.Problems += 1;

				IDeviceTelemetry lastProblem = problems[0];

				DeviceReport deviceReport = new DeviceReport(platform.Id.ToString(), platform.Name, device.Id.ToString(), device.Name, device.Address ?? "Unknown Address", pool.Id.ToString(), pool.Name, telemetry);

				deviceReport.ProblemDelta = problems.Count;

				if (lastProblem.JobName != null && lastProblem.StepName != null && lastProblem.JobId != null && lastProblem.StepId != null)
				{
					deviceReport.LastProblemDesc = $"{lastProblem.JobName} - {lastProblem.StepName}";
					deviceReport.LastProblemURL = new Uri($"{globalConfig.ServerSettings.DashboardUrl}job/{lastProblem.JobId}?step={lastProblem.StepId}").ToString();
				}

				DevicePlatformReport? platformReport = issueReport.PlatformReports.Find(p => p.PlatformId == device.PlatformId.ToString());
				if (platformReport == null)
				{
					platformReport = new DevicePlatformReport(platform.Id.ToString(), platform.Name);
					issueReport.PlatformReports.Add(platformReport);
				}

				platformReport.DeviceReports.Add(deviceReport);
			});

			// order everything nicely
			issueReport.PoolReports = issueReport.PoolReports.OrderBy(r => r.PoolName).ToList();
			issueReport.PlatformReports = issueReport.PlatformReports.OrderBy(r => r.PlatformName).ToList();
			issueReport.PlatformReports.ForEach(r => r.DeviceReports = r.DeviceReports.OrderBy(r => r.PoolName).ThenBy(r => r.DeviceName).ToList());

			if (issueReport.PoolReports.Count > 0 || issueReport.PlatformReports.Count > 0)
			{
				try
				{
					_logger.LogInformation("Sending device report notification");

					await _notificationService.SendDeviceIssueReportAsync(issueReport, cancellationToken);
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Error sending device health notification");
				}
			}

			state = await _state.UpdateAsync(s => s.ReportTime = currentTime, cancellationToken);

		}
	}
}

// CL 22278596 - Has the device channel notification stuff