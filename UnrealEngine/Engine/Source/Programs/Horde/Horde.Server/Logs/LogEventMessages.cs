// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Text.Json;
using EpicGames.Core;
using HordeCommon;

namespace Horde.Server.Logs
{
	/// <summary>
	/// Information about an uploaded event
	/// </summary>
	public class GetLogEventResponse
	{
		/// <summary>
		/// Unique id of the log containing this event
		/// </summary>
		public string LogId { get; set; }

		/// <summary>
		/// Severity of this event
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
		/// The issue id associated with this event
		/// </summary>
		public int? IssueId { get; set; }

		/// <summary>
		/// The structured message data for this event
		/// </summary>
		public List<JsonElement> Lines { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="logEvent">The event to construct from</param>
		/// <param name="eventData">The event data</param>
		/// <param name="issueId">The issue for this event</param>
		public GetLogEventResponse(ILogEvent logEvent, ILogEventData eventData, int? issueId)
		{
			Severity = logEvent.Severity;
			LogId = logEvent.LogId.ToString();
			LineIndex = logEvent.LineIndex;
			LineCount = logEvent.LineCount;
			IssueId = issueId;
			Lines = eventData.Lines.ConvertAll(x => JsonDocument.Parse(x.Data).RootElement);
		}
	}
}
