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
	/// Instance of a particular compile error
	/// </summary>
	class ContentIssueHandler : IssueHandler
	{
		/// <inheritdoc/>
		public override string Type => "Content";

		/// <inheritdoc/>
		public override int Priority => 10;

		/// <summary>
		/// Determines if the given event id matches
		/// </summary>
		/// <param name="eventId">The event id to compare</param>
		/// <returns>True if the given event id matches</returns>
		public static bool IsMatchingEventId(EventId eventId)
		{
			return eventId == KnownLogEvents.Engine_AssetLog;
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
		/// Adds all the assets from the given log event
		/// </summary>
		/// <param name="eventData">The log event to parse</param>
		/// <param name="assetNames">Receives the referenced asset names</param>
		public static void GetAssetNames(ILogEventData eventData, HashSet<string> assetNames)
		{
			foreach (ILogEventLine line in eventData.Lines)
			{
				string? relativePath;
				if (line.Data.TryGetNestedProperty("properties.asset.relativePath", out relativePath))
				{
					int endIdx = relativePath.LastIndexOfAny(new char[] { '/', '\\' }) + 1;
					string fileName = relativePath.Substring(endIdx);
					assetNames.Add(fileName);
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
						HashSet<string> newAssetNames = new HashSet<string>();
						GetAssetNames(stepEvent.EventData, newAssetNames);

						stepEvent.Fingerprint = new NewIssueFingerprint(Type, newAssetNames, null, null);
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
		public override string GetSummary(IIssueFingerprint fingerprint, IssueSeverity severity)
		{
			string type = (severity == IssueSeverity.Warning) ? "Warnings" : "Errors";
			string list = StringUtils.FormatList(fingerprint.Keys.ToArray(), 2);
			return $"{type} in {list}";
		}

		/// <inheritdoc/>
		public override void RankSuspects(IIssueFingerprint fingerprint, List<SuspectChange> suspects)
		{
			foreach (SuspectChange suspect in suspects)
			{
				if (suspect.ContainsContent)
				{
					if (suspect.Details.Files.Any(x => fingerprint.Keys.Any(y => x.Path.Contains(y, StringComparison.OrdinalIgnoreCase))))
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
	}
}
