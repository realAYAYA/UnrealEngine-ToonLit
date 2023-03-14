// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Build.Server
{
	/// <summary>
	/// Interface for a service which keeps track of whether we're during downtime
	/// </summary>
	public interface IDowntimeService
	{
		/// <summary>
		/// Returns true if downtime is currently active
		/// </summary>
		public bool IsDowntimeActive
		{
			get;
		}
	}

	/// <summary>
	/// Service which manages the downtime schedule
	/// </summary>
	public sealed class DowntimeService : IDowntimeService, IHostedService, IDisposable
	{
		/// <summary>
		/// Whether the server is currently in downtime
		/// </summary>
		public bool IsDowntimeActive
		{
			get;
			private set;
		}

		readonly MongoService _mongoService;
		readonly ITicker _ticker;
		readonly IOptionsMonitor<ServerSettings> _settings;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service instance</param>
		/// <param name="clock"></param>
		/// <param name="settings">The server settings</param>
		/// <param name="logger">Logger instance</param>
		public DowntimeService(MongoService mongoService, IClock clock, IOptionsMonitor<ServerSettings> settings, ILogger<DowntimeService> logger)
		{
			_mongoService = mongoService;
			_settings = settings;
			_logger = logger;

			// Ensure the initial value to be correct
			TickAsync(CancellationToken.None).AsTask().Wait();

			_ticker = clock.AddTicker<DowntimeService>(TimeSpan.FromMinutes(1.0), TickAsync, logger);
		}

		/// <inheritdoc/>
		public Task StartAsync(CancellationToken cancellationToken) => _ticker.StartAsync();

		/// <inheritdoc/>
		public Task StopAsync(CancellationToken cancellationToken) => _ticker.StopAsync();

		/// <inheritdoc/>
		public void Dispose() => _ticker.Dispose();

		/// <summary>
		/// Periodically called tick function
		/// </summary>
		/// <param name="stoppingToken">Token indicating that the service should stop</param>
		/// <returns>Async task</returns>
		async ValueTask TickAsync(CancellationToken stoppingToken)
		{
			Globals globals = await _mongoService.GetGlobalsAsync();

			DateTimeOffset now = TimeZoneInfo.ConvertTime(DateTimeOffset.Now, _settings.CurrentValue.TimeZoneInfo);
			bool bIsActive = globals.ScheduledDowntime.Any(x => x.IsActive(now));

			DateTimeOffset? next = null;
			foreach (ScheduledDowntime schedule in globals.ScheduledDowntime)
			{
				DateTimeOffset start = schedule.GetNext(now).StartTime;
				if (next == null || start < next)
				{
					next = start;
				}
			}

			if (next != null)
			{
				_logger.LogInformation("Server time: {Time}. Downtime: {Downtime}. Next: {Next}.", now, bIsActive, next.Value);
			}

			if (bIsActive != IsDowntimeActive)
			{
				IsDowntimeActive = bIsActive;
				if (IsDowntimeActive)
				{
					_logger.LogInformation("Entering downtime");
				}
				else
				{
					_logger.LogInformation("Leaving downtime");
				}
			}
		}
	}
}
