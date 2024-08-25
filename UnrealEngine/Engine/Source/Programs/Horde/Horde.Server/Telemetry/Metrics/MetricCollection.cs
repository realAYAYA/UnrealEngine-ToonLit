// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Telemetry;
using EpicGames.Horde.Telemetry.Metrics;
using Horde.Server.Server;
using Horde.Server.Utilities;
using HordeCommon;
using Json.Path;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;
using TDigestNet;

namespace Horde.Server.Telemetry.Metrics
{
	class MetricCollection : IMetricCollection, IHostedService, IAsyncDisposable
	{
		class MetricDocument : IMetric
		{
			public ObjectId Id { get; set; }

			[BsonElement("ts")]
			public TelemetryStoreId TelemetryStoreId { get; set; }

			[BsonElement("met")]
			public MetricId MetricId { get; set; }

			[BsonElement("grp")]
			public string Group { get; set; } = String.Empty;

			[BsonElement("time")]
			public DateTime Time { get; set; }

			[BsonElement("value")]
			public double Value { get; set; }

			[BsonElement("count")]
			public int Count { get; set; }

			[BsonElement("state")]
			public byte[]? State { get; set; }
		}

		record class SampleKey(TelemetryStoreId Store, MetricId Metric, string Group, DateTime Time);

		readonly object _lockObject = new object();
		readonly IMongoCollection<MetricDocument> _metrics;
		readonly AsyncEvent _newDataEvent = new AsyncEvent();
		readonly AsyncEvent _flushEvent = new AsyncEvent();
		readonly BackgroundTask _flushTask;
		readonly IClock _clock;
		readonly IOptionsMonitor<GlobalConfig> _globalConfig;
		readonly ILogger _logger;

		Dictionary<SampleKey, List<double>> _queuedSamples = new Dictionary<SampleKey, List<double>>();

		public MetricCollection(MongoService mongoService, IClock clock, IOptionsMonitor<GlobalConfig> globalConfig, ILogger<MetricCollection> logger)
		{
			List<MongoIndex<MetricDocument>> indexes = new List<MongoIndex<MetricDocument>>();
			indexes.Add(MongoIndex.Create<MetricDocument>(keys => keys.Ascending(x => x.TelemetryStoreId).Descending(x => x.Time).Ascending(x => x.MetricId).Ascending(x => x.Group)));
			_metrics = mongoService.GetCollection<MetricDocument>("Metrics", indexes);

			_flushTask = new BackgroundTask(BackgroundTickAsync);
			_clock = clock;
			_globalConfig = globalConfig;
			_logger = logger;
		}

		/// <inheritdoc/>
		public Task StartAsync(CancellationToken cancellationToken)
		{
			_flushTask.Start();
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public async Task StopAsync(CancellationToken cancellationToken)
		{
			await _flushTask.StopAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			await _flushTask.DisposeAsync();
		}

		/// <inheritdoc/>
		public void AddEvent(TelemetryStoreId storeId, JsonNode node)
		{
			JsonArray array = new JsonArray { node };

			TelemetryStoreConfig? telemetryStoreConfig;
			if (_globalConfig.CurrentValue.TryGetTelemetryStore(storeId, out telemetryStoreConfig))
			{
				foreach (MetricConfig metric in telemetryStoreConfig.Metrics)
				{
					AddEvent(storeId, metric, node, array);
				}
			}

			array.Remove(node);
		}

		void AddEvent(TelemetryStoreId telemetryStoreId, MetricConfig metric, JsonNode node, JsonArray array)
		{
			if (metric.Filter != null)
			{
				PathResult filterResult = metric.Filter.Evaluate(array);
				if (filterResult.Error != null)
				{
					_logger.LogWarning("Error evaluating filter for metric {MetricId}: {Message}", metric.Id, filterResult.Error);
					return;
				}
				if (filterResult.Matches == null || filterResult.Matches.Count == 0)
				{
					return;
				}
			}

			List<double> values = new List<double>();

			if (metric.Property != null)
			{
				PathResult result = metric.Property.Evaluate(node);
				if (result.Error != null)
				{
					_logger.LogWarning("Error evaluating filter for metric {MetricId}: {Message}", metric.Id, result.Error);
					return;
				}

				if (result.Matches != null && result.Matches.Count > 0)
				{

					foreach (Json.Path.Node match in result.Matches)
					{
						JsonValue? value = match.Value as JsonValue;
						if (value != null && value.TryGetValue(out double doubleValue))
						{
							values.Add(doubleValue);
						}
					}
				}
			}
			else
			{
				if (metric.Function != AggregationFunction.Count)
				{
					_logger.LogWarning("Missing property parameter for metric {MetricId}", metric.Id);
					return;
				}

				values.Add(1);
			}

			if (values.Count > 0)
			{
				List<string> groupKeys = new List<string>();
				foreach (JsonPath groupByPath in metric.GroupByPaths)
				{
					PathResult groupResult = groupByPath.Evaluate(node);

					string groupKey;
					if (groupResult.Error != null || groupResult.Matches == null || groupResult.Matches.Count == 0)
					{
						groupKey = "";
					}
					else
					{
						groupKey = EscapeCsv(groupResult.Matches.Select(x => x.Value?.ToString() ?? String.Empty));
					}

					groupKeys.Add(groupKey);
				}

				string group = String.Join(",", groupKeys);

				DateTime utcNow = _clock.UtcNow;
				DateTime sampleTime = new DateTime(utcNow.Ticks - (utcNow.Ticks % metric.Interval.Ticks), DateTimeKind.Utc);

				SampleKey key = new SampleKey(telemetryStoreId, metric.Id, group.ToString(), sampleTime);
				lock (_lockObject)
				{
					QueueSampleValues(key, values);
				}

				_newDataEvent.Set();
			}
		}

		static string EscapeCsv(IEnumerable<string> items)
		{
			return String.Join(",", items.Select(x => EscapeCsv(x)));
		}

		static readonly char[] s_csvEscapeChars = { ',', '\n', '\"' };

		static string? EscapeCsv(string text)
		{
			if (text.IndexOfAny(s_csvEscapeChars) != -1)
			{
				text = text.Replace("\"", "\"\"", StringComparison.Ordinal);
				text = $"\"{text}\"";
			}
			return text;
		}

		void QueueSampleValues(SampleKey key, List<double> values)
		{
			List<double>? queuedValues;
			if (!_queuedSamples.TryGetValue(key, out queuedValues))
			{
				queuedValues = new List<double>();
				_queuedSamples.Add(key, queuedValues);
			}
			queuedValues.AddRange(values);
		}

		async Task BackgroundTickAsync(CancellationToken cancellationToken)
		{
			Task newDataTask = _newDataEvent.Task;
			Task flushTask = _flushEvent.Task;

			while (!cancellationToken.IsCancellationRequested)
			{
				await newDataTask.WaitAsync(cancellationToken);
				await Task.WhenAny(flushTask, Task.Delay(TimeSpan.FromSeconds(5.0), cancellationToken));

				newDataTask = _newDataEvent.Task;
				flushTask = _flushEvent.Task;

				await FlushAsync(cancellationToken);
			}
		}

		public async Task FlushAsync(CancellationToken cancellationToken)
		{
			// Copy the current sample buffer
			Dictionary<SampleKey, List<double>> samples;
			lock (_lockObject)
			{
				samples = _queuedSamples;
				_queuedSamples = new Dictionary<SampleKey, List<double>>();
			}

			// Add the samples to the database
			try
			{
				foreach ((SampleKey sampleKey, List<double> sampleValues) in samples)
				{
					MetricConfig? metricConfig;
					if (_globalConfig.CurrentValue.TryGetTelemetryStore(sampleKey.Store, out TelemetryStoreConfig? telemetryStoreConfig) && telemetryStoreConfig.TryGetMetric(sampleKey.Metric, out metricConfig))
					{
						await CombineValuesAsync(sampleKey.Store, metricConfig, sampleKey.Group, sampleKey.Time, sampleValues, cancellationToken);
					}
				}
			}
			catch
			{
				lock (_lockObject)
				{
					foreach ((SampleKey key, List<double> values) in samples)
					{
						QueueSampleValues(key, values);
					}
				}
				throw;
			}
		}

		async Task CombineValuesAsync(TelemetryStoreId telemetryStoreId, MetricConfig metricConfig, string group, DateTime time, List<double> values, CancellationToken cancellationToken)
		{
			for (; ; )
			{
				// Find or add the current metric document
				FilterDefinition<MetricDocument> filter = Builders<MetricDocument>.Filter.Expr(x => x.TelemetryStoreId == telemetryStoreId && x.MetricId == metricConfig.Id && x.Group == group && x.Time == time);
				UpdateDefinition<MetricDocument> update = Builders<MetricDocument>.Update.SetOnInsert(x => x.Count, 0);
				MetricDocument metric = await _metrics.FindOneAndUpdateAsync(filter, update, new FindOneAndUpdateOptions<MetricDocument, MetricDocument> { IsUpsert = true, ReturnDocument = ReturnDocument.After }, cancellationToken);

				// Combine the samples
				TDigest digest = (metric.Count == 0) ? new TDigest() : TDigest.Deserialize(metric.State);
				foreach (double value in values)
				{
					digest.Add(value);
				}
				metric.State = digest.Serialize();

				// Save the previous count of samples to sequence updates to the document
				int prevCount = metric.Count;
				metric.Count += values.Count;

				// Update the current value
				switch (metricConfig.Function)
				{
					case AggregationFunction.Count:
						metric.Value = metric.Count;
						break;
					case AggregationFunction.Min:
						metric.Value = digest.Min;
						break;
					case AggregationFunction.Max:
						metric.Value = digest.Max;
						break;
					case AggregationFunction.Sum:
						metric.Value += values.Sum();
						break;
					case AggregationFunction.Average:
						metric.Value = digest.Average;
						break;
					case AggregationFunction.Percentile:
						metric.Value = digest.Quantile(metricConfig.Percentile / 100.0);
						break;
					default:
						_logger.LogWarning("Unhandled aggregation function '{Function}'", metricConfig.Function);
						break;
				}

				// Update the document
				ReplaceOneResult result = await _metrics.ReplaceOneAsync(x => x.TelemetryStoreId == telemetryStoreId && x.Id == metric.Id && x.Count == prevCount, metric, cancellationToken: cancellationToken);
				if (result.MatchedCount > 0)
				{
					break;
				}
			}
		}

		/// <inheritdoc/>
		public async Task<List<IMetric>> FindAsync(TelemetryStoreId telemetryStoreId, MetricId[] metricIds, DateTime? minTime = null, DateTime? maxTime = null, string? group = null, int maxResults = 50, CancellationToken cancellationToken = default)
		{
			FilterDefinition<MetricDocument> filter = FilterDefinition<MetricDocument>.Empty;
			filter &= Builders<MetricDocument>.Filter.Eq(x => x.TelemetryStoreId, telemetryStoreId);

			if (minTime != null)
			{
				filter &= Builders<MetricDocument>.Filter.Gte(x => x.Time, minTime.Value);
			}
			if (maxTime != null)
			{
				filter &= Builders<MetricDocument>.Filter.Lte(x => x.Time, maxTime.Value);
			}

			filter &= Builders<MetricDocument>.Filter.In(x => x.MetricId, metricIds);

			if (group != null)
			{
				filter &= Builders<MetricDocument>.Filter.Eq(x => x.Group, group);
			}

			List<MetricDocument> results = await _metrics.Find(filter).SortByDescending(x => x.Time).Limit(maxResults).ToListAsync(cancellationToken);
			return results.ConvertAll<MetricDocument, IMetric>(x => x);
		}
	}
}
