// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using EpicGames.Core;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Logs;
using Horde.Server.Utilities;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Issues.Handlers
{
	/// <summary>
	/// Instance of a particular compile error
	/// </summary>
	class SymbolIssueHandler : IssueHandler
	{
		const string NodeName = "Node";
		const string EventIdName = "EventId";

		static readonly string s_duplicateEventIdMetadata = $"{EventIdName}={KnownLogEvents.Linker_DuplicateSymbol.Id}";

		/// <inheritdoc/>
		public override string Type => "Symbol";

		/// <inheritdoc/>
		public override int Priority => 10;

		/// <summary>
		/// Determines if the given event id matches
		/// </summary>
		/// <param name="eventId">The event id to compare</param>
		/// <returns>True if the given event id matches</returns>
		public static bool IsMatchingEventId(EventId eventId)
		{
			return eventId == KnownLogEvents.Linker_UndefinedSymbol || eventId == KnownLogEvents.Linker_DuplicateSymbol || eventId == KnownLogEvents.Linker;
		}

		/// <summary>
		/// Determines if an event should be masked by this 
		/// </summary>
		/// <param name="eventId"></param>
		/// <returns></returns>
		static bool IsMaskedEventId(EventId eventId)
		{
			return eventId == KnownLogEvents.ExitCode || eventId == KnownLogEvents.Systemic_Xge_BuildFailed;
		}

		/// <summary>
		/// Parses symbol names from a log event
		/// </summary>
		/// <param name="eventData">The log event data</param>
		/// <param name="symbolNames">Receives the list of symbol names</param>
		public static void GetSymbolNames(ILogEventData eventData, SortedSet<string> symbolNames)
		{
			foreach (ILogEventLine line in eventData.Lines)
			{
				string? identifier;
				if (line.Data.TryGetNestedProperty("properties.symbol.identifier", out identifier))
				{
					symbolNames.Add(identifier);
				}
			}
		}

		/// <inheritdoc/>
		public override void RankSuspects(IIssueFingerprint fingerprint, List<SuspectChange> changes)
		{
			HashSet<string> names = new HashSet<string>();
			foreach (string name in fingerprint.Keys)
			{
				names.UnionWith(name.Split("::", StringSplitOptions.RemoveEmptyEntries));
			}

			foreach (SuspectChange change in changes)
			{
				if (change.ContainsCode)
				{
					int matches = names.Count(x => change.Files.Any(y => y.Contains(x, StringComparison.OrdinalIgnoreCase)));
					change.Rank += 10 + (10 * matches);
				}
			}
		}

		/// <inheritdoc/>
		public override string GetSummary(IIssueFingerprint fingerprint, IssueSeverity severity)
		{
			HashSet<string> symbols = fingerprint.Keys;
			if (symbols.Count == 0)
			{
				string[] nodes = fingerprint.GetMetadataValues(NodeName).ToArray();

				StringBuilder summary = new StringBuilder("Linker ");
				summary.Append((severity == IssueSeverity.Warning) ? "warnings" : "errors");
				if (nodes.Length > 0)
				{
					summary.Append($" in {StringUtils.FormatList(nodes, 2)}");
				}

				return summary.ToString();
			}
			else
			{
				string problemType = fingerprint.HasMetadataValue(s_duplicateEventIdMetadata) ? "Duplicate" : "Undefined";
				if (symbols.Count == 1)
				{
					return $"{problemType} symbol '{symbols.First()}'";
				}
				else
				{
					return $"{problemType} symbols: {StringUtils.FormatList(symbols.ToArray(), 3)}";
				}
			}
		}

		public override void TagEvents(IJob job, INode node, IReadOnlyNodeAnnotations annotations, IReadOnlyList<IssueEvent> stepEvents)
		{
			bool hasMatches = false;
			foreach (IssueEvent stepEvent in stepEvents)
			{
				if (stepEvent.EventId != null)
				{
					EventId eventId = stepEvent.EventId.Value;
					if (IsMatchingEventId(eventId))
					{
						SortedSet<string> symbolNames = new SortedSet<string>();
						GetSymbolNames(stepEvent.EventData, symbolNames);

						if (symbolNames.Count > 0)
						{
							List<string> metadata = new List<string>();
							metadata.Add($"{NodeName}={node.Name}");
							metadata.Add($"{EventIdName}={eventId.Id}");

							stepEvent.Fingerprint = new NewIssueFingerprint(Type, symbolNames, null, metadata);
							hasMatches = true;
						}
						else if (hasMatches)
						{
							stepEvent.Ignored = true;
						}
					}
					else if (hasMatches && IsMaskedEventId(eventId))
					{
						stepEvent.Ignored = true;
					}
				}
			}
		}
	}
}
