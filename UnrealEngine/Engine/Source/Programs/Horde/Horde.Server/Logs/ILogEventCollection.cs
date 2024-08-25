// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Logs;
using MongoDB.Bson;

namespace Horde.Server.Logs
{
	/// <summary>
	/// Interface for a collection of event documents
	/// </summary>
	public interface ILogEventCollection
	{
		/// <summary>
		/// Creates a new event
		/// </summary>
		/// <param name="newEvent">The new event to vreate</param>
		Task AddAsync(NewLogEventData newEvent);

		/// <summary>
		/// Creates a new event
		/// </summary>
		/// <param name="newEvents">List of events to create</param>
		Task AddManyAsync(List<NewLogEventData> newEvents);

		/// <summary>
		/// Finds events within a log file
		/// </summary>
		/// <param name="logId">Unique id of the log containing this event</param>
		/// <param name="spanId">Optional span to filter events by</param>
		/// <param name="index">Start index within the matching results</param>
		/// <param name="count">Maximum number of results to return</param>
		/// <returns>List of events matching the query</returns>
		Task<List<ILogEvent>> FindAsync(LogId logId, ObjectId? spanId, int? index = null, int? count = null);

		/// <summary>
		/// Finds a list of events for a set of spans
		/// </summary>
		/// <param name="spanIds">The span ids</param>
		/// <param name="logIds">List of log ids to query</param>
		/// <param name="index">Index of the first result to return</param>
		/// <param name="count">Number of results to return</param>
		/// <returns>List of events for this issue</returns>
		Task<List<ILogEvent>> FindEventsForSpansAsync(IEnumerable<ObjectId> spanIds, LogId[]? logIds = null, int index = 0, int count = 10);

		/// <summary>
		/// Delete all the events for a log file
		/// </summary>
		/// <param name="logId">Unique id of the log</param>
		/// <returns>Async task</returns>
		Task DeleteLogAsync(LogId logId);

		/// <summary>
		/// Update the span for an event
		/// </summary>
		/// <param name="events">The events to update</param>
		/// <param name="spanId">New span id</param>
		/// <returns>Async task</returns>
		Task AddSpanToEventsAsync(IEnumerable<ILogEvent> events, ObjectId spanId);
	}
}
