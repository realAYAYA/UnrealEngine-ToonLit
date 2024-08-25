// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Issues.Handlers
{
	/// <summary>
	/// Instance of a particular compile error
	/// </summary>
	[IssueHandler(Priority = 1)]
	public class HashedIssueHandler : IssueHandler
	{
		readonly IssueHandlerContext _context;
		readonly List<IssueEvent> _issueEvents = new List<IssueEvent>();

		/// <summary>
		/// Known general events
		/// </summary>
		static readonly HashSet<EventId> s_knownGeneralEvents = new HashSet<EventId> { KnownLogEvents.Generic, KnownLogEvents.ExitCode, KnownLogEvents.Horde, KnownLogEvents.Horde_InvalidPreflight };

		/// <summary>
		/// Constructor
		/// </summary>
		public HashedIssueHandler(IssueHandlerContext context) => _context = context;

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
		public override bool HandleEvent(IssueEvent logEvent)
		{
			_issueEvents.Add(logEvent);
			return true;
		}

		/// <inheritdoc/>
		public override IEnumerable<IssueEventGroup> GetIssues()
		{
			List<IssueEventGroup> issues = new List<IssueEventGroup>();

			IssueEventGroup? genericFingerprint = null;
			HashSet<Md5Hash> hashes = new HashSet<Md5Hash>();

			// keep hash consistent when only have general, non-unique events
			bool allGeneral = _issueEvents.FirstOrDefault(stepEvent => stepEvent.EventId == null || !IsGeneralEventId(stepEvent.EventId.Value)) == null;

			foreach (IssueEvent stepEvent in _issueEvents)
			{
				string hashSource = stepEvent.Message;

				if (!allGeneral && stepEvent.EventId != null)
				{
					// If the event is general, salt the hash with the stream id, template, otherwise it will be aggressively matched.
					// Consider salting with node name, though template id should be enough and have better grouping
					if (IsGeneralEventId(stepEvent.EventId.Value))
					{
						hashSource += $"step:{_context.StreamId}:{_context.TemplateId}";
					}
				}

				if (hashes.Count < 25 && TryGetHash(hashSource, out Md5Hash hash))
				{
					hashes.Add(hash);

					IssueEventGroup issue = new IssueEventGroup("Hashed", "{Severity} in {Meta:Node}", IssueChangeFilter.All);
					issue.Events.Add(stepEvent);
					issue.Keys.AddHash(hash);
					issue.Metadata.Add("Node", _context.NodeName);
					issues.Add(issue);
				}
				else
				{
					if (genericFingerprint == null)
					{
						genericFingerprint = new IssueEventGroup("Hashed", "{Severity} in {Meta:Node}", IssueChangeFilter.All);
						genericFingerprint.Keys.Add(IssueKey.FromStep(_context.StreamId, _context.TemplateId, _context.NodeName));
						genericFingerprint.Metadata.Add("Node", _context.NodeName);
						issues.Add(genericFingerprint);
					}
					genericFingerprint.Events.Add(stepEvent);
				}
			}

			return issues;
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
	}
}
