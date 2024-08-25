// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.Json;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Issues.Handlers
{
	/// <summary>
	/// Instance of a localization error
	/// </summary>
	[IssueHandler(Priority = 10)]
	public class LocalizationIssueHandler : IssueHandler
	{
		readonly List<IssueEventGroup> _issues = new List<IssueEventGroup>();

		/// <summary>
		/// Determines if the given event id matches
		/// </summary>
		/// <param name="eventId">The event id to compare</param>
		/// <returns>True if the given event id matches</returns>
		public static bool IsMatchingEventId(EventId? eventId)
		{
			return eventId == KnownLogEvents.Engine_Localization;
		}

		/// <summary>
		/// Determines if an event should be masked by this 
		/// </summary>
		/// <param name="eventId"></param>
		/// <returns></returns>
		static bool IsMaskedEventId(EventId eventId)
		{
			return eventId == KnownLogEvents.ExitCode;
		}

		/// <summary>
		/// Extracts a list of source files from an event
		/// </summary>
		/// <param name="issueEvent">The event data</param>
		/// <param name="sourceFiles">List of source files</param>
		public static void GetSourceFiles(IssueEvent issueEvent, HashSet<IssueKey> sourceFiles)
		{
			foreach (JsonLogEvent line in issueEvent.Lines)
			{
				JsonDocument document = JsonDocument.Parse(line.Data);

				string? relativePath;
				if (document.RootElement.TryGetNestedProperty("properties.file.relativePath", out relativePath) || document.RootElement.TryGetNestedProperty("properties.file", out relativePath))
				{
					if (!relativePath.EndsWith(".manifest", StringComparison.OrdinalIgnoreCase))
					{
						int endIdx = relativePath.LastIndexOfAny(new char[] { '/', '\\' }) + 1;
						string fileName = relativePath.Substring(endIdx);
						sourceFiles.Add(new IssueKey(fileName, IssueKeyType.File));
					}
				}
			}
		}

		/// <inheritdoc/>
		public override bool HandleEvent(IssueEvent logEvent)
		{
			if (logEvent.EventId != null)
			{
				EventId eventId = logEvent.EventId.Value;
				if (IsMatchingEventId(eventId))
				{
					IssueEventGroup issue = new IssueEventGroup("Localization", "Localization {Severity} in {Files}", IssueChangeFilter.Code);
					issue.Events.Add(logEvent);
					GetSourceFiles(logEvent, issue.Keys);

					if (issue.Keys.Count > 0)
					{
						_issues.Add(issue);
					}

					return true;
				}
				else if (_issues.Count > 0 && IsMaskedEventId(eventId))
				{
					return true;
				}
			}
			return false;
		}

		/// <inheritdoc/>
		public override IEnumerable<IssueEventGroup> GetIssues() => _issues;
	}
}
