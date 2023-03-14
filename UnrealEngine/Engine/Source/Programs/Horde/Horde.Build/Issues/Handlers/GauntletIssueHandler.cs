// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Text.Json;
using EpicGames.Core;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Logs;
using Horde.Build.Utilities;
using Microsoft.Extensions.Logging;

namespace Horde.Build.Issues.Handlers
{
	/// <summary>
	/// Instance of a particular Gauntlet error
	/// </summary>
	class GauntletIssueHandler : IssueHandler
	{
		/// <summary>
		/// Prefix for unit test keys
		/// </summary>
		const string UnitTestPrefix = "UnitTest:";

		/// <summary>
		/// Prefix for screenshot test keys
		/// </summary>
		const string ScreenshotTestPrefix = "Screenshot:";

		/// <inheritdoc/>
		public override string Type => "Gauntlet";

		/// <inheritdoc/>
		public override int Priority => 10;

		/// <summary>
		/// Determines if the given event id matches
		/// </summary>
		/// <param name="eventId">The event id to compare</param>
		/// <returns>True if the given event id matches</returns>
		public static bool IsMatchingEventId(EventId eventId)
		{
			return eventId == KnownLogEvents.Gauntlet_UnitTest || eventId == KnownLogEvents.Gauntlet_ScreenshotTest;
		}

		/// <summary>
		/// Parses symbol names from a log event
		/// </summary>
		/// <param name="eventData">The log event data</param>
		/// <param name="unitTestNames">Receives a set of the unit test names</param>
		private static void GetUnitTestNames(ILogEventData eventData, HashSet<string> unitTestNames)
		{
			foreach (ILogEventLine line in eventData.Lines)
			{
				string? group = null;
				string? name = null;

				string? value;
				if (line.Data.TryGetNestedProperty("properties.group", out value))
				{
					group = value;
				}
				if (line.Data.TryGetNestedProperty("properties.name", out value))
				{
					name = value;
				}

				if (group != null && name != null)
				{
					unitTestNames.Add($"{UnitTestPrefix}:{group}/{name}");
				}
			}
		}

		/// <summary>
		/// Parses screenshot test names from a log event
		/// </summary>
		/// <param name="eventData">The event data</param>
		/// <param name="screenshotTestNames">Receives the parsed screenshot test names</param>
		private static void GetScreenshotTestNames(ILogEventData eventData, HashSet<string> screenshotTestNames)
		{
			foreach (ILogEventLine line in eventData.Lines)
			{
				if (line.Data.TryGetProperty("properties", JsonValueKind.Object, out JsonElement properties))
				{
					foreach (JsonProperty property in properties.EnumerateObject())
					{
						JsonElement value = property.Value;
						if (value.ValueKind == JsonValueKind.Object && value.HasStringProperty("type", "Screenshot") && value.TryGetStringProperty("name", out string? name))
						{
							screenshotTestNames.Add($"{ScreenshotTestPrefix}:{name}");
						}
					}
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
					HashSet<string> keys = new HashSet<string>();
					GetUnitTestNames(stepEvent.EventData, keys);
					GetScreenshotTestNames(stepEvent.EventData, keys);

					if (keys.Count == 0)
					{
						stepEvent.Ignored = true;
					}
					else
					{
						stepEvent.Fingerprint = new NewIssueFingerprint(Type, keys, null, null);
					}
				}
			}
		}

		/// <inheritdoc/>
		public override void RankSuspects(IIssueFingerprint fingerprint, List<SuspectChange> changes)
		{
			foreach (SuspectChange change in changes)
			{
				if (change.ContainsCode)
				{
					change.Rank += 10;
				}
			}
		}

		/// <inheritdoc/>
		public override string GetSummary(IIssueFingerprint fingerprint, IssueSeverity severity)
		{
			List<string> unitTestNames = fingerprint.Keys.Where(x => x.StartsWith(UnitTestPrefix, StringComparison.Ordinal)).Select(x => x.Substring(UnitTestPrefix.Length + 1)).ToList();
			if (unitTestNames.Count > 0)
			{
				HashSet<string> groupNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
				HashSet<string> testNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
				foreach (string unitTestName in unitTestNames)
				{
					int idx = unitTestName.IndexOf('/', StringComparison.OrdinalIgnoreCase);
					if (idx != -1)
					{
						groupNames.Add(unitTestName.Substring(0, idx));
					}
					testNames.Add(unitTestName.Substring(idx + 1));
				}

				if (groupNames.Count == 1)
				{
					return $"{groupNames.First()} test failures: {StringUtils.FormatList(testNames.ToArray(), 3)}";
				}
				else
				{
					return $"{StringUtils.FormatList(groupNames.OrderBy(x => x).ToArray(), 100)} test failures";
				}
			}

			List<string> screenshotTestNames = fingerprint.Keys.Where(x => x.StartsWith(ScreenshotTestPrefix, StringComparison.Ordinal)).Select(x => x.Substring(ScreenshotTestPrefix.Length + 1)).ToList();
			if (screenshotTestNames.Count > 0)
			{
				return $"Screenshot test failures: {StringUtils.FormatList(screenshotTestNames.ToArray(), 3)}";
			}

			return "Test failures";
		}
	}
}
