// Copyright Epic Games, Inc. All Rights Reserved.

namespace Horde.Build.Telemetry
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
}
