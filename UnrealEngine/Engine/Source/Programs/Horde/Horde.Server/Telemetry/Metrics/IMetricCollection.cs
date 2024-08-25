// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Telemetry;
using EpicGames.Horde.Telemetry.Metrics;

namespace Horde.Server.Telemetry.Metrics
{
	/// <summary>
	/// Collection of aggregated metrics
	/// </summary>
	public interface IMetricCollection
	{
		/// <summary>
		/// Adds a new event to the collection
		/// </summary>
		/// <param name="storeId">Identifier for a telemetry store</param>
		/// <param name="node">Arbitrary event node</param>
		void AddEvent(TelemetryStoreId storeId, JsonNode node);

		/// <summary>
		/// Finds metrics over a given time period
		/// </summary>
		/// <param name="storeId">Identifier for a telemetry store</param>
		/// <param name="metricIds">Metrics to search for</param>
		/// <param name="minTime">Start of the time period to query</param>
		/// <param name="maxTime">End of the time period to query</param>
		/// <param name="group">Grouping key to filter results</param>
		/// <param name="maxResults">Maximum number of results to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task<List<IMetric>> FindAsync(TelemetryStoreId storeId, MetricId[] metricIds, DateTime? minTime = null, DateTime? maxTime = null, string? group = null, int maxResults = 50, CancellationToken cancellationToken = default);

		/// <summary>
		/// Flush all the pending events to storage
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task FlushAsync(CancellationToken cancellationToken);
	}
}
