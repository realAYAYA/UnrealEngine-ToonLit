// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Telemetry;

namespace Horde.Server.Telemetry
{
	/// <summary>
	/// Interface for a telemetry sink
	/// </summary>
	public interface ITelemetrySink
	{
		/// <summary>
		/// Whether the sink is enabled
		/// </summary>
		bool Enabled { get; }

		/// <summary>
		/// Sends a telemetry event with the given information
		/// </summary>
		/// <param name="storeId">Identifier for the telemetry store</param>
		/// <param name="telemetryEvent">The telemetry event that was received</param>
		void SendEvent(TelemetryStoreId storeId, TelemetryEvent telemetryEvent);
	}

	/// <summary>
	/// Internal interface for sinks
	/// </summary>
	public interface ITelemetrySinkInternal : ITelemetrySink, IAsyncDisposable
	{
		/// <summary>
		/// Flush any queued metrics to underlying service
		/// </summary>
		/// <param name="cancellationToken"></param>
		/// <returns>Completion task</returns>
		public ValueTask FlushAsync(CancellationToken cancellationToken);
	}

	/// <summary>
	/// Extension methods for telemetry sinks
	/// </summary>
	public static class TelemetrySinkExtensions
	{
		/// <summary>
		/// Sends a telemetry event with the given information
		/// </summary>
		public static void SendEvent(this ITelemetrySink sink, TelemetryStoreId storeId, TelemetryRecordMeta recordMeta, object payload)
		{
			sink.SendEvent(storeId, new TelemetryEvent(recordMeta, payload));
		}
	}
}

