// Copyright Epic Games, Inc. All Rights Reserved.

namespace Horde.Server.Telemetry
{
	/// <summary>
	/// Wrapper around native telemetry objects
	/// </summary>
	public class TelemetryEvent
	{
		/// <summary>
		/// Record metadata
		/// </summary>
		public TelemetryRecordMeta RecordMeta { get; set; }

		/// <summary>
		/// Accessor for the native object
		/// </summary>
		public object Payload { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public TelemetryEvent(TelemetryRecordMeta recordMeta, object payload)
		{
			RecordMeta = recordMeta;
			Payload = payload;
		}
	}

	/// <summary>
	/// Additional metadata associated with a telemetry event
	/// </summary>
	/// <param name="AppId">Identifier of the application sending the event</param>
	/// <param name="AppVersion">Version number of the application</param>
	/// <param name="AppEnvironment">Name of the environment that the sending application is running in</param>
	/// <param name="SessionId">Unique identifier for the current session</param>
	public record class TelemetryRecordMeta(string? AppId = null, string? AppVersion = null, string? AppEnvironment = null, string? SessionId = null)
	{
		/// <summary>
		/// Record metadata for the running Horde instance
		/// </summary>
		public static TelemetryRecordMeta CurrentHordeInstance { get; } = new TelemetryRecordMeta
		{
			AppId = "Horde",
			AppVersion = ServerApp.Version.ToString(),
			AppEnvironment = ServerApp.DeploymentEnvironment,
			SessionId = ServerApp.SessionId
		};
	}
}
