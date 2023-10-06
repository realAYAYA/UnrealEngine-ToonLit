// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;

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
		/// <param name="eventName">Name of the event</param>
		/// <param name="attributes">Arbitrary object to include in the payload</param>
		void SendEvent(string eventName, object attributes);
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
}
