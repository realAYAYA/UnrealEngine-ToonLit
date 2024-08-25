// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using EpicGames.Horde.Telemetry.Metrics;

namespace Horde.Server.Telemetry.Metrics
{
	/// <summary>
	/// Interface for a metric event
	/// </summary>
	public interface IMetric
	{
		/// <summary>
		/// Unique identifier for this event
		/// </summary>
		MetricId MetricId { get; }

		/// <summary>
		/// Start time for this aggregation interval
		/// </summary>
		DateTime Time { get; }

		/// <summary>
		/// Grouping key for this metric
		/// </summary>
		string? Group { get; }

		/// <summary>
		/// Aggregated value for the metric
		/// </summary>
		double Value { get; }

		/// <summary>
		/// Number of samples
		/// </summary>
		int Count { get; }
	}
}
