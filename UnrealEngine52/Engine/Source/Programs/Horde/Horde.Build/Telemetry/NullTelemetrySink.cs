// Copyright Epic Games, Inc. All Rights Reserved.

namespace Horde.Build.Telemetry
{
	/// <summary>
	/// Telemetry sink that discards all events
	/// </summary>
	public sealed class NullTelemetrySink : ITelemetrySink
	{
		/// <inheritdoc/>
		public bool Enabled => false;

		/// <inheritdoc/>
		public void SendEvent(string eventName, object attributes)
		{
		}
	}
}
