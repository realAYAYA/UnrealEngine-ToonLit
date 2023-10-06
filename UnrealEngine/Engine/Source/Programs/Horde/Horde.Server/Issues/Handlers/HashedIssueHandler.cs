// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using EpicGames.Core;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Graphs;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Issues.Handlers
{
	/// <summary>
	/// Instance of a particular compile error
	/// </summary>
	class HashedIssueHandler : IssueHandler
	{
		const string NodeName = "Node";

		/// <inheritdoc/>
		public override string Type => "Hashed";

		/// <inheritdoc/>
		public override int Priority => 1;

		/// <summary>
		///  Known general events
		/// </summary>
		static readonly HashSet<EventId> s_knownGeneralEvents = new HashSet<EventId> { KnownLogEvents.Generic, KnownLogEvents.ExitCode, KnownLogEvents.Horde, KnownLogEvents.Horde_InvalidPreflight };

		/// <summary>
		/// Determines if the given event is general and should be salted to make it unique
		/// </summary>
		/// <param name="eventId">The event id to compare</param>
		/// <returns>True if the given event id matches</returns>
		public static bool IsGeneralEventId(EventId eventId)
		{
			return s_knownGeneralEvents.Contains(eventId) || (eventId.Id >= KnownLogEvents.Systemic.Id && eventId.Id <= KnownLogEvents.Systemic_Max.Id);
		}

		/// <inheritdoc/>
		public override void TagEvents(IJob job, INode node, IReadOnlyNodeAnnotations annotations, IReadOnlyList<IssueEvent> stepEvents)
		{
			string[] metadata = new[] { $"{NodeName}={node.Name}" };

			NewIssueFingerprint? genericFingerprint = null;
			HashSet<Md5Hash> hashes = new HashSet<Md5Hash>();

			// keep hash consistent when only have general, non-unique events
			bool allGeneral = stepEvents.FirstOrDefault(stepEvent => stepEvent.EventId == null || !IsGeneralEventId(stepEvent.EventId.Value)) == null;

			foreach (IssueEvent stepEvent in stepEvents)
			{
				string hashSource = stepEvent.Message;
				
				if (!allGeneral && stepEvent.EventId != null)
				{
					// If the event is general, salt the hash with the stream id, template, otherwise it will be aggressively matched.
					// Consider salting with node name, though template id should be enough and have better grouping
					if (IsGeneralEventId(stepEvent.EventId.Value))
					{
						hashSource += $"step:{job.StreamId}:{job.TemplateId}";
					}					
				}

				if (hashes.Count < 25 && TryGetHash(hashSource, out Md5Hash hash))
				{
					hashes.Add(hash);
					stepEvent.Fingerprint = new NewIssueFingerprint(Type, new[] { $"hash:{hash}" }, null, metadata);
				}
				else
				{
					genericFingerprint ??= new NewIssueFingerprint(Type, new[] { $"step:{job.StreamId}:{job.TemplateId}:{node.Name}" }, null, metadata);
					stepEvent.Fingerprint = genericFingerprint;
				}
			}
		}

		static bool TryGetHash(string message, out Md5Hash hash)
		{
			string sanitized = message.ToUpperInvariant();
			sanitized = Regex.Replace(sanitized, @"(?<![a-zA-Z])(?:[A-Z]:|/)[^ :]+[/\\]SYNC[/\\]", "{root}/"); // Redact things that look like workspace roots; may be different between agents
			sanitized = Regex.Replace(sanitized, @"0[xX][0-9a-fA-F]+", "H"); // Redact hex strings
			sanitized = Regex.Replace(sanitized, @"\d[\d.,:]*", "n"); // Redact numbers and timestamp like things

			if (sanitized.Length > 30)
			{
				hash = Md5Hash.Compute(Encoding.UTF8.GetBytes(sanitized));
				return true;
			}
			else
			{
				hash = Md5Hash.Zero;
				return false;
			}
		}

		/// <inheritdoc/>
		public override void RankSuspects(IIssueFingerprint fingerprint, List<SuspectChange> suspects)
		{
		}

		/// <inheritdoc/>
		public override string GetSummary(IIssueFingerprint fingerprint, IssueSeverity severity)
		{
			string severityText = (severity == IssueSeverity.Warning) ? "Warnings" : "Errors";
			string[] nodeNames = fingerprint.GetMetadataValues(NodeName).ToArray();
			return $"{severityText} in {StringUtils.FormatList(nodeNames, 2)}";
		}
	}
}
