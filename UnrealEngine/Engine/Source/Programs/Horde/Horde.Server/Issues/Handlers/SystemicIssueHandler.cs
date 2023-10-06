// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using EpicGames.Core;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Logs;
using HordeCommon;
using Microsoft.Extensions.Logging;
using MongoDB.Driver;

namespace Horde.Server.Issues.Handlers
{
	/// <summary>
	/// Instance of a particular systemic error
	/// </summary>
	class SystemicIssueHandler : IssueHandler
	{
		const string NodeNameKey = "Node";

		/// <inheritdoc/>
		public override string Type => "Systemic";

		/// <inheritdoc/>
		public override int Priority => 10;

		/// <summary>
		///  Known systemic errors
		/// </summary>
		static readonly HashSet<EventId> s_knownSystemic = new HashSet<EventId> { KnownLogEvents.Horde, KnownLogEvents.Horde_InvalidPreflight };

		/// <summary>
		/// Determines if the given event id matches
		/// </summary>
		/// <param name="eventId">The event id to compare</param>
		/// <returns>True if the given event id matches</returns>
		public static bool IsMatchingEventId(EventId eventId)
		{
			return s_knownSystemic.Contains(eventId) || (eventId.Id >= KnownLogEvents.Systemic.Id && eventId.Id <= KnownLogEvents.Systemic_Max.Id);
		}

		/// <inheritdoc/>
		public override void RankSuspects(IIssueFingerprint fingerprint, List<SuspectChange> suspects)
		{
			suspects.Clear();
		}

		/// <inheritdoc/>
		public override void TagEvents(IJob job, INode node, IReadOnlyNodeAnnotations annotations, IReadOnlyList<IssueEvent> stepEvents)
		{
			NewIssueFingerprint? fingerprint = null;
			bool nonSystemicError = false;
			foreach (IssueEvent stepEvent in stepEvents)
			{
				if (stepEvent.EventId == null)
				{					
					continue;
				}

				if (MatchEvent(stepEvent.EventData))
				{
					if (nonSystemicError)
					{						
						stepEvent.Ignored = true;
					}
					else
					{
						fingerprint ??= new NewIssueFingerprint(Type, new[] { $"step:{job.StreamId}:{job.TemplateId}:{node.Name}" }, null, new[] { $"{NodeNameKey}={node.Name}" });
						stepEvent.Fingerprint = fingerprint;
					}
				}
				else
				{					
					if (stepEvent.Severity == EventSeverity.Error)
					{
						// We've seen a non-systemic error event, so ignore this systemic event to prevent superfluous issues from being created
						nonSystemicError = true;
					}
					
				}
			}
		}

		static bool MatchEvent(ILogEventData eventData)
		{
			return eventData.EventId != null && IsMatchingEventId(eventData.EventId.Value);
		}

		/// <inheritdoc/>
		public override string GetSummary(IIssueFingerprint fingerprint, IssueSeverity severity)
		{
			string type = (severity == IssueSeverity.Warning) ? "Systemic warnings" : "Systemic errors";
			string nodeName = fingerprint.GetMetadataValues(NodeNameKey).FirstOrDefault() ?? "(unknown)";
			return $"{type} in {nodeName}";
		}
	}
}
