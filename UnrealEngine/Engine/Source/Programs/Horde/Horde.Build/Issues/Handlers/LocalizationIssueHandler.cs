// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using EpicGames.Core;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Logs;
using Horde.Build.Utilities;
using Microsoft.Extensions.Logging;
using MongoDB.Driver;

namespace Horde.Build.Issues.Handlers
{
	/// <summary>
	/// Instance of a localization error
	/// </summary>
	class LocalizationIssueHandler : IssueHandler
	{
		/// <inheritdoc/>
		public override string Type => "Localization";

		/// <inheritdoc/>
		public override int Priority => 10;

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
		/// <param name="logEventData">The event data</param>
		/// <param name="sourceFiles">List of source files</param>
		public static void GetSourceFiles(ILogEventData logEventData, HashSet<string> sourceFiles)
		{
			foreach (ILogEventLine line in logEventData.Lines)
			{
				string? relativePath;
				if (line.Data.TryGetNestedProperty("properties.file.relativePath", out relativePath) || line.Data.TryGetNestedProperty("properties.file", out relativePath))
				{
					if (!relativePath.EndsWith(".manifest", StringComparison.OrdinalIgnoreCase))
					{
						int endIdx = relativePath.LastIndexOfAny(new char[] { '/', '\\' }) + 1;
						string fileName = relativePath.Substring(endIdx);
						sourceFiles.Add(fileName);
					}
				}
			}
		}

		/// <inheritdoc/>
		public override void TagEvents(IJob job, INode node, IReadOnlyNodeAnnotations annotations, IReadOnlyList<IssueEvent> stepEvents)
		{
			bool hasMatches = false;
			foreach (IssueEvent stepEvent in stepEvents)
			{
				if (stepEvent.EventId != null)
				{
					if (IsMatchingEventId(stepEvent.EventId.Value))
					{
						HashSet<string> newFileNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
						GetSourceFiles(stepEvent.EventData, newFileNames);

						if (newFileNames.Count == 0)
						{
							stepEvent.Ignored = true;
						}
						else
						{
							stepEvent.Fingerprint = new NewIssueFingerprint(Type, newFileNames, null, null);
						}

						hasMatches = true;
					}
					else if (hasMatches && IsMaskedEventId(stepEvent.EventId.Value))
					{
						stepEvent.Ignored = true;
					}
				}
			}
		}

		/// <inheritdoc/>
		public override void RankSuspects(IIssueFingerprint fingerprint, List<SuspectChange> suspects)
		{
			foreach (SuspectChange suspect in suspects)
			{
				if (suspect.ContainsCode)
				{
					if (fingerprint.Keys.Any(x => suspect.ModifiesFile(x)))
					{
						suspect.Rank += 20;
					}
					else
					{
						suspect.Rank += 10;
					}
				}
			}
		}

		/// <inheritdoc/>
		public override string GetSummary(IIssueFingerprint fingerprint, IssueSeverity severity)
		{
			string type = (severity == IssueSeverity.Warning)? "warnings" : "errors";
			return $"Localization {type} in {StringUtils.FormatList(fingerprint.Keys.ToArray(), 2)}";
		}
	}
}
