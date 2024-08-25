// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Text.Json;
using EpicGames.Core;
using EpicGames.Horde.Logs;
using HordeCommon;
using Microsoft.Extensions.Logging;
using MongoDB.Bson;

namespace Horde.Server.Logs
{
	/// <summary>
	/// Interface for event data
	/// </summary>
	public interface ILogEventData
	{
		/// <summary>
		/// The log level
		/// </summary>
		EventSeverity Severity { get; }

		/// <summary>
		/// The type of event
		/// </summary>
		EventId? EventId { get; }

		/// <summary>
		/// The complete rendered message, in plaintext
		/// </summary>
		string Message { get; }

		/// <summary>
		/// Gets this event data as a JSON objects
		/// </summary>
		IReadOnlyList<JsonLogEvent> Lines { get; }
	}

	/// <summary>
	/// Represents a node in the graph
	/// </summary>
	public interface ILogEvent
	{
		/// <summary>
		/// Unique id of the log containing this event
		/// </summary>
		public LogId LogId { get; }

		/// <summary>
		/// Severity of the event
		/// </summary>
		public EventSeverity Severity { get; }

		/// <summary>
		/// Index of the first line for this event
		/// </summary>
		public int LineIndex { get; }

		/// <summary>
		/// Number of lines in the event
		/// </summary>
		public int LineCount { get; }

		/// <summary>
		/// Span id for this log event
		/// </summary>
		public ObjectId? SpanId { get; }
	}

	/// <summary>
	/// Represents a node in the graph
	/// </summary>
	public class NewLogEventData
	{
		/// <summary>
		/// Unique id of the log containing this event
		/// </summary>
		public LogId LogId { get; set; }

		/// <summary>
		/// Severity of the event
		/// </summary>
		public EventSeverity Severity { get; set; }

		/// <summary>
		/// Index of the first line for this event
		/// </summary>
		public int LineIndex { get; set; }

		/// <summary>
		/// Number of lines in the event
		/// </summary>
		public int LineCount { get; set; }

		/// <summary>
		/// The span this this event belongs to
		/// </summary>
		public ObjectId? SpanId { get; set; }
	}

	/// <summary>
	/// Extensions for parsing log event properties
	/// </summary>
	public static class LogEventExtensions
	{
		/// <summary>
		/// Find all properties of the given type in a particular log line
		/// </summary>
		/// <param name="data">Line data</param>
		/// <param name="type">Type of property to return</param>
		/// <returns></returns>
		public static IEnumerable<JsonProperty> FindPropertiesOfType(this ILogEventData data, Utf8String type)
		{
			return data.Lines.SelectMany(x => x.FindPropertiesOfType(type));
		}
	}
}
