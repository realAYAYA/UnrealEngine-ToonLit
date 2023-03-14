// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
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
	class UnacceptableWordsIssueHandler : SourceFileIssueHandler
	{
		/// <inheritdoc/>
		public override string Type => "UnacceptableWords";

		/// <inheritdoc/>
		public override int Priority => 8;

		/// <summary>
		/// Determines if the given event id matches
		/// </summary>
		/// <param name="eventId">The event id to compare</param>
		/// <returns>True if the given event id matches</returns>
		public static bool IsMatchingEventId(EventId eventId)
		{
			return eventId == KnownLogEvents.AutomationTool_UnacceptableWords;
		}

		/// <inheritdoc/>
		public override string GetSummary(IIssueFingerprint fingerprint, IssueSeverity severity)
		{
			return $"Unacceptable words in {StringUtils.FormatList(fingerprint.Keys.Select(x => x.Substring(x.LastIndexOf('/') + 1)).ToArray(), 3)}";
		}

		/// <inheritdoc/>
		public override void TagEvents(IJob job, INode node, IReadOnlyNodeAnnotations annotations, IReadOnlyList<IssueEvent> stepEvents)
		{
			foreach (IssueEvent stepEvent in stepEvents)
			{
				if(stepEvent.EventId != null && IsMatchingEventId(stepEvent.EventId.Value))
				{
					List<string> newFileNames = new List<string>();
					GetSourceFiles(stepEvent.EventData, newFileNames);

					stepEvent.Fingerprint = new NewIssueFingerprint(Type, newFileNames, null, null);
				}
			}
		}
	}
}
