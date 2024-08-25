// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Horde.Logs;
using Horde.Server.Server;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.Extensions.Logging;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Server.Logs
{
	/// <summary>
	/// Collection of event documents
	/// </summary>
	public class LogEventCollection : ILogEventCollection
	{
		class LogEventId
		{
			[BsonElement("l")]
			public LogId LogId { get; set; }

			[BsonElement("n")]
			public int LineIndex { get; set; }
		}

		class LogEventDocument : ILogEvent
		{
			[BsonId]
			public LogEventId Id { get; set; }

			[BsonElement("w"), BsonIgnoreIfDefault, BsonDefaultValue(false)]
			public bool IsWarning { get; set; }

			[BsonElement("c"), BsonIgnoreIfNull]
			public int? LineCount { get; set; }

			[BsonElement("s")]
			public ObjectId? SpanId { get; set; }

			LogId ILogEvent.LogId => Id.LogId;
			int ILogEvent.LineIndex => Id.LineIndex;
			int ILogEvent.LineCount => LineCount ?? 1;
			EventSeverity ILogEvent.Severity => IsWarning ? EventSeverity.Warning : EventSeverity.Error;

			public LogEventDocument()
			{
				Id = new LogEventId();
			}

			public LogEventDocument(LogId logId, EventSeverity severity, int lineIndex, int lineCount, ObjectId? spanId)
			{
				Id = new LogEventId { LogId = logId, LineIndex = lineIndex };
				IsWarning = severity == EventSeverity.Warning;
				LineCount = (lineCount > 1) ? (int?)lineCount : null;
				SpanId = spanId;
			}

			public LogEventDocument(NewLogEventData data)
				: this(data.LogId, data.Severity, data.LineIndex, data.LineCount, data.SpanId)
			{
			}
		}

		class LegacyEventDocument
		{
			public ObjectId Id { get; set; }
			public DateTime Time { get; set; }
			public EventSeverity Severity { get; set; }
			public LogId LogId { get; set; }
			public int LineIndex { get; set; }
			public int LineCount { get; set; }

			public string? Message { get; set; }

			[BsonIgnoreIfNull, BsonElement("IssueId2")]
			public int? IssueId { get; set; }

			public BsonDocument? Data { get; set; }

			public int UpgradeVersion { get; set; }
		}

		/// <summary>
		/// Collection of event documents
		/// </summary>
		readonly IMongoCollection<LogEventDocument> _logEvents;

		/// <summary>
		/// Collection of legacy event documents
		/// </summary>
		readonly IMongoCollection<LegacyEventDocument> _legacyEvents;

		/// <summary>
		/// Logger for upgrade messages
		/// </summary>
		readonly ILogger<LogEventCollection> _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service instance</param>
		/// <param name="logger">The logger instance</param>
		public LogEventCollection(MongoService mongoService, ILogger<LogEventCollection> logger)
		{
			_logger = logger;

			List<MongoIndex<LogEventDocument>> logEventIndexes = new List<MongoIndex<LogEventDocument>>();
			logEventIndexes.Add(keys => keys.Ascending(x => x.Id.LogId));
			logEventIndexes.Add(keys => keys.Ascending(x => x.SpanId).Ascending(x => x.Id));
			_logEvents = mongoService.GetCollection<LogEventDocument>("LogEvents", logEventIndexes);

			_legacyEvents = mongoService.GetCollection<LegacyEventDocument>("Events", keys => keys.Ascending(x => x.LogId));
		}

		/// <inheritdoc/>
		public Task AddAsync(NewLogEventData newEvent)
		{
			return _logEvents.InsertOneAsync(new LogEventDocument(newEvent));
		}

		/// <inheritdoc/>
		public Task AddManyAsync(List<NewLogEventData> newEvents)
		{
			return _logEvents.InsertManyAsync(newEvents.ConvertAll(x => new LogEventDocument(x)));
		}

		/// <inheritdoc/>
		public async Task<List<ILogEvent>> FindAsync(LogId logId, ObjectId? spanId = null, int? index = null, int? count = null)
		{
			_logger.LogInformation("Querying for log events for log {LogId}", logId);

			FilterDefinitionBuilder<LogEventDocument> builder = Builders<LogEventDocument>.Filter;

			FilterDefinition<LogEventDocument> filter = builder.Eq(x => x.Id.LogId, logId);
			if (spanId != null)
			{
				filter &= builder.Eq(x => x.SpanId, spanId.Value);
			}

			IFindFluent<LogEventDocument, LogEventDocument> results = _logEvents.Find(filter).SortBy(x => x.Id);
			if (index != null)
			{
				results = results.Skip(index.Value);
			}
			if (count != null)
			{
				results = results.Limit(count.Value);
			}

			List<LogEventDocument> eventDocuments = await results.ToListAsync();
			return eventDocuments.ConvertAll<ILogEvent>(x => x);
		}

		/// <inheritdoc/>
		public async Task<List<ILogEvent>> FindEventsForSpansAsync(IEnumerable<ObjectId> spanIds, LogId[]? logIds, int index, int count)
		{
			FilterDefinition<LogEventDocument> filter = Builders<LogEventDocument>.Filter.In(x => x.SpanId, spanIds.Select<ObjectId, ObjectId?>(x => x));
			if (logIds != null && logIds.Length > 0)
			{
				filter &= Builders<LogEventDocument>.Filter.In(x => x.Id.LogId, logIds);
			}

			List<LogEventDocument> eventDocuments = await _logEvents.Find(filter).Skip(index).Limit(count).ToListAsync();
			return eventDocuments.ConvertAll<ILogEvent>(x => x);
		}

		/// <inheritdoc/>
		public async Task DeleteLogAsync(LogId logId)
		{
			await _logEvents.DeleteManyAsync(x => x.Id.LogId == logId);
			await _legacyEvents.DeleteManyAsync(x => x.LogId == logId);
		}

		/// <inheritdoc/>
		public async Task AddSpanToEventsAsync(IEnumerable<ILogEvent> events, ObjectId spanId)
		{
			FilterDefinition<LogEventDocument> eventFilter = Builders<LogEventDocument>.Filter.In(x => x.Id, events.OfType<LogEventDocument>().Select(x => x.Id));
			UpdateDefinition<LogEventDocument> eventUpdate = Builders<LogEventDocument>.Update.Set(x => x.SpanId, spanId);
			await _logEvents.UpdateManyAsync(eventFilter, eventUpdate);
		}
	}
}
