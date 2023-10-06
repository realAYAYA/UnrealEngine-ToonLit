// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using OpenTelemetry.Trace;

namespace Horde.Server.Telemetry
{
	/// <summary>
	/// Telemetry sink dispatching incoming events to all registered sinks
	/// </summary>
	public sealed class TelemetryManager : ITelemetrySinkInternal, IHostedService
	{
		/// <inheritdoc/>
		public bool Enabled => _telemetrySinks.Count > 0;

		private readonly List<ITelemetrySinkInternal> _telemetrySinks = new();
		private readonly ITicker _ticker;
		private readonly Tracer _tracer;
		private readonly ILogger<TelemetryManager> _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="httpClientFactory"></param>
		/// <param name="clock"></param>
		/// <param name="serverSettings"></param>
		/// <param name="tracer"></param>
		/// <param name="loggerFactory"></param>
		public TelemetryManager(IHttpClientFactory httpClientFactory, IClock clock, IOptions<ServerSettings> serverSettings, Tracer tracer, ILoggerFactory loggerFactory)
		{
			_tracer = tracer;
			_logger = loggerFactory.CreateLogger<TelemetryManager>();
			_ticker = clock.AddTicker<EpicTelemetrySink>(TimeSpan.FromSeconds(30.0), FlushAsync, _logger);
			
			foreach (BaseTelemetryConfig config in serverSettings.Value.Telemetry)
			{
				switch (config)
				{
					case EpicTelemetryConfig epicConfig:
						_telemetrySinks.Add(new EpicTelemetrySink(epicConfig, httpClientFactory, loggerFactory.CreateLogger<EpicTelemetrySink>()));
						break;
					case ClickHouseTelemetryConfig chConfig:
						_telemetrySinks.Add(new ClickHouseTelemetrySink(chConfig, httpClientFactory, loggerFactory.CreateLogger<ClickHouseTelemetrySink>()));
						break;
				}
			}
		}

		/// <inheritdoc/>
		public void SendEvent(string eventName, object attributes)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(TelemetryManager)}.{nameof(SendEvent)}");
			span.SetAttribute("eventName", eventName);
			
			foreach (ITelemetrySinkInternal sink in _telemetrySinks)
			{
				using TelemetrySpan sinkSpan = _tracer.StartActiveSpan($"SendEvent");
				string fullName = sink.GetType().FullName ?? "Unknown";
				span.SetAttribute("sink", fullName);
				
				if (sink.Enabled)
				{
					try
					{
						sink.SendEvent(eventName, attributes);
					}
					catch (Exception e)
					{
						_logger.LogError(e, "Failed sending event to {Sink}. Message: {Message}", fullName, e.Message);
					}
				}
			}
		}

		/// <inheritdoc />
		public async Task StartAsync(CancellationToken cancellationToken)
		{
			await _ticker.StartAsync();
		}

		/// <inheritdoc />
		public async Task StopAsync(CancellationToken cancellationToken)
		{
			await _ticker.StopAsync();
		}

		/// <inheritdoc />
		public async ValueTask DisposeAsync()
		{
			_ticker.Dispose();
			foreach (ITelemetrySinkInternal sink in _telemetrySinks)
			{
				await sink.DisposeAsync();
			}
		}

		/// <inheritdoc />
		public async ValueTask FlushAsync(CancellationToken cancellationToken)
		{
			foreach (ITelemetrySinkInternal sink in _telemetrySinks)
			{
				await sink.FlushAsync(cancellationToken);
			}
		}
	}
}
