// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace Horde.Build.Agents.Telemetry
{
	/// <summary>
	/// Collection of utilization collection
	/// </summary>
	public interface ITelemetryCollection
	{
		/// <summary>
		/// Adds entries for the given utilization
		/// </summary>
		/// <param name="telemetry">Telemetry data to add</param>
		/// <returns>Async task</returns>
		Task AddUtilizationTelemetryAsync(IUtilizationTelemetry telemetry);

		/// <summary>
		/// Finds utilization data matching the given criteria
		/// </summary>
		/// <param name="startTimeUtc">Start time to query utilization for</param>
		/// <param name="finishTimeUtc">Finish time to query utilization for</param>
		/// <returns>The utilization data</returns>
		Task<List<IUtilizationTelemetry>> GetUtilizationTelemetryAsync(DateTime startTimeUtc, DateTime finishTimeUtc);

		/// <summary>
		/// Finds the latest utilization data
		/// </summary>
		/// <returns>The utilization data</returns>
		Task<IUtilizationTelemetry?> GetLatestUtilizationTelemetryAsync();
	}
}
