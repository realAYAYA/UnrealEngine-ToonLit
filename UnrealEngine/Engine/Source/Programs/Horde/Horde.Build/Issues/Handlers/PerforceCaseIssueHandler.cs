// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Text.Json;
using EpicGames.Core;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Logs;
using Microsoft.Extensions.Logging;

namespace Horde.Build.Issues.Handlers
{
	/// <summary>
	/// Instance of a Perforce case mismatch error
	/// </summary>
	class PerforceCaseIssueHandler : IssueHandler
	{
		/// <inheritdoc/>
		public override string Type => "PerforceCase";

		/// <inheritdoc/>
		public override int Priority => 10;

		/// <summary>
		/// Determines if the given event id matches
		/// </summary>
		/// <param name="eventId">The event id to compare</param>
		/// <returns>True if the given event id matches</returns>
		public static bool IsMatchingEventId(EventId eventId)
		{
			return eventId == KnownLogEvents.AutomationTool_PerforceCase;
		}

		/// <inheritdoc/>
		public override void RankSuspects(IIssueFingerprint fingerprint, List<SuspectChange> changes)
		{
			foreach (SuspectChange change in changes)
			{
				if(change.Details.Files.Any(x => fingerprint.Keys.Contains(x.DepotPath)))
				{
					change.Rank += 30;
				}
			}
		}

		/// <inheritdoc/>
		public override string GetSummary(IIssueFingerprint fingerprint, IssueSeverity severity)
		{
			return $"Inconsistent case for {StringUtils.FormatList(fingerprint.Keys.Select(x => x.Substring(x.LastIndexOf('/') + 1)).ToArray(), 3)}";
		}

		/// <summary>
		/// Extracts a list of source files from an event
		/// </summary>
		/// <param name="logEventData">The event data</param>
		/// <param name="depotFiles">List of source files</param>
		static void GetSourceFiles(ILogEventData logEventData, HashSet<string> depotFiles)
		{
			foreach (JsonProperty property in logEventData.FindPropertiesOfType(LogValueType.DepotPath))
			{
				JsonElement value;
				if (property.Value.TryGetProperty(LogEventPropertyName.Text.Span, out value) && value.ValueKind == JsonValueKind.String)
				{
					depotFiles.Add(value.GetString()!);
				}
			}
		}

		/// <inheritdoc/>
		public override void TagEvents(IJob job, INode node, IReadOnlyNodeAnnotations annotations, IReadOnlyList<IssueEvent> stepEvents)
		{
			foreach (IssueEvent stepEvent in stepEvents)
			{
				if (stepEvent.EventId != null && IsMatchingEventId(stepEvent.EventId.Value))
				{
					HashSet<string> newFileNames = new HashSet<string>();
					GetSourceFiles(stepEvent.EventData, newFileNames);

					stepEvent.Fingerprint = new NewIssueFingerprint(Type, newFileNames, null, null);
				}
			}
		}
	}
}
