// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Text.Json;
using EpicGames.Core;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.Extensions.Logging;
using MongoDB.Bson;

namespace Horde.Build.Logs
{
	using LogId = ObjectId<ILogFile>;

	/// <summary>
	/// Interface for a log event line
	/// </summary>
	public interface ILogEventLine
	{
		/// <summary>
		/// Log level
		/// </summary>
		LogLevel Level { get; }

		/// <summary>
		/// The event id
		/// </summary>
		EventId? EventId { get; }

		/// <summary>
		/// The complete rendered message, in plaintext
		/// </summary>
		string Message { get; }

		/// <summary>
		/// Data for the line
		/// </summary>
		JsonElement Data { get; }
	}

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
		/// Gets this event data as a BSON document
		/// </summary>
		IReadOnlyList<ILogEventLine> Lines { get; }
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
			return data.Lines.SelectMany(x => FindPropertiesOfType(x, type));
		}

		/// <summary>
		/// Find all properties of the given type in a particular log line
		/// </summary>
		/// <param name="line">Line data</param>
		/// <param name="type">Type of property to return</param>
		/// <returns></returns>
		public static IEnumerable<JsonProperty> FindPropertiesOfType(this ILogEventLine line, Utf8String type)
		{
			JsonElement properties;
			if (line.Data.TryGetProperty("properties", out properties) && properties.ValueKind == JsonValueKind.Object)
			{
				foreach (JsonProperty property in properties.EnumerateObject())
				{
					if (property.Value.ValueKind == JsonValueKind.Object)
					{
						foreach (JsonProperty subProperty in property.Value.EnumerateObject())
						{
							if (subProperty.NameEquals(LogEventPropertyName.Type.Span))
							{
								if (subProperty.Value.ValueKind == JsonValueKind.String && subProperty.Value.ValueEquals(type.Span))
								{
									yield return property;
								}
								else
								{
									break;
								}
							}
						}
					}
				}
			}
		}
	}
}
