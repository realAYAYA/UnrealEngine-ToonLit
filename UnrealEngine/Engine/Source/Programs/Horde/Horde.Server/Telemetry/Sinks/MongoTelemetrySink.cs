// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Telemetry;
using Horde.Server.Server;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Server.Telemetry.Sinks
{
	/// <summary>
	/// Telemetry sink which writes data to a MongoDB collection
	/// </summary>
	public sealed class MongoTelemetrySink : ITelemetrySinkInternal, IHostedService, IAsyncDisposable
	{
		class EventDocument
		{
			public ObjectId Id { get; set; }

			[BsonElement("ts")]
			public TelemetryStoreId TelemetryStoreId { get; set; }

			[BsonElement("data")]
			public BsonDocument Data { get; set; }

			[BsonConstructor]
			public EventDocument()
			{
				Data = new BsonDocument();
			}

			public EventDocument(TelemetryStoreId telemetryStoreId, BsonDocument data)
			{
				Id = ObjectId.GenerateNewId();
				TelemetryStoreId = telemetryStoreId;
				Data = data;
			}
		}

		readonly MongoTelemetryConfig? _config;
		readonly IMongoCollection<EventDocument> _telemetry;
		readonly AsyncEvent _newDataEvent = new AsyncEvent();
		readonly AsyncEvent _flushEvent = new AsyncEvent();
		readonly ConcurrentQueue<EventDocument> _queue = new ConcurrentQueue<EventDocument>();
		readonly BackgroundTask _backgroundTask;
		readonly ITicker _cleanupTicker;
		readonly JsonSerializerOptions _jsonOptions;
		readonly ILogger _logger;

		/// <inheritdoc/>
		public bool Enabled => _config != null;

		/// <summary>
		/// Constructor
		/// </summary>
		public MongoTelemetrySink(MongoService mongoService, IClock clock, IOptions<ServerSettings> serverSettings, ILogger<MongoTelemetrySink> logger)
		{
			_config = serverSettings.Value.Telemetry.Select(x => x as MongoTelemetryConfig).FirstOrDefault(x => x != null);
			_telemetry = mongoService.GetCollection<EventDocument>("Telemetry", builder => builder.Ascending(x => x.TelemetryStoreId).Descending(x => x.Id));
			_backgroundTask = new BackgroundTask(BackgroundFlushAsync);
			_cleanupTicker = clock.AddSharedTicker<MongoTelemetrySink>(TimeSpan.FromHours(4.0), CleanupAsync, logger);
			_logger = logger;

			_jsonOptions = new JsonSerializerOptions();
			Startup.ConfigureJsonSerializer(_jsonOptions);
		}

		/// <inheritdoc/>
		public async Task StartAsync(CancellationToken cancellationToken)
		{
			if (Enabled)
			{
				_backgroundTask.Start();
				await _cleanupTicker.StartAsync();
			}
		}

		/// <inheritdoc/>
		public async Task StopAsync(CancellationToken cancellationToken)
		{
			await _cleanupTicker.StopAsync();
			await _backgroundTask.StopAsync(cancellationToken);
		}

		// Flushes the sink in the background
		async Task BackgroundFlushAsync(CancellationToken cancellationToken)
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

		async ValueTask CleanupAsync(CancellationToken cancellationToken)
		{
			if (_config == null)
			{
				return;
			}

			DateTime baseTime = DateTime.UtcNow - TimeSpan.FromDays(_config.RetainDays);
#pragma warning disable CS0618 // Type or member is obsolete
			ObjectId minObjectId = new ObjectId(baseTime, 0, 0, 0);
#pragma warning restore CS0618
			DeleteResult result = await _telemetry.DeleteManyAsync(x => x.Id < minObjectId, cancellationToken);
			_logger.LogInformation("Deleted {NumItems} telemetry records", result.DeletedCount);
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			await _cleanupTicker.DisposeAsync();
			await _backgroundTask.DisposeAsync();
			await FlushAsync(default);
		}

		/// <inheritdoc/>
		public async ValueTask FlushAsync(CancellationToken cancellationToken)
		{
			// Copy all the event documents from the queue
			List<EventDocument> eventDocuments = new List<EventDocument>(_queue.Count);
			while (_queue.TryDequeue(out EventDocument? eventDocument))
			{
				eventDocuments.Add(eventDocument);
			}

			// Insert them into the database
			if (eventDocuments.Count > 0)
			{
				_logger.LogInformation("Writing {NumEvents} new telemetry events to MongoDB.", eventDocuments.Count);
				await _telemetry.InsertManyAsync(eventDocuments, cancellationToken: cancellationToken);
			}
		}

		/// <inheritdoc/>
		public void SendEvent(TelemetryStoreId telemetryStoreId, TelemetryEvent telemetryEvent)
		{
			BsonDocument bson = BsonDocument.Parse(JsonSerializer.Serialize(telemetryEvent, _jsonOptions));
			_queue.Enqueue(new EventDocument(telemetryStoreId, bson));
			_newDataEvent.Set();

			const int FlushCount = 50;
			if (_queue.Count > FlushCount)
			{
				_flushEvent.Set();
			}
		}
	}
}
