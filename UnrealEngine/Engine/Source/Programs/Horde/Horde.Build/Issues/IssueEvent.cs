// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Text;
using EpicGames.Core;
using Horde.Build.Logs;
using HordeCommon;
using Microsoft.Extensions.Logging;

namespace Horde.Build.Issues
{
	/// <summary>
	/// Wraps a log event and allows it to be tagged by issue handlers
	/// </summary>
	public class IssueEvent
	{
		/// <summary>
		/// The underlying log event
		/// </summary>
		public ILogEvent Event { get; }

		/// <summary>
		/// The log event data
		/// </summary>
		public ILogEventData EventData { get; }

		/// <summary>
		/// Severity of the event
		/// </summary>
		public EventSeverity Severity => EventData.Severity;

		/// <summary>
		/// The type of event
		/// </summary>
		public EventId? EventId => EventData.EventId;

		/// <summary>
		/// The complete rendered message, in plaintext
		/// </summary>
		public string Message => EventData.Message;

		/// <summary>
		/// Gets this event data as a BSON document
		/// </summary>
		public IReadOnlyList<ILogEventLine> Lines => EventData.Lines;

		/// <summary>
		/// Fingerprint assigned to this issue
		/// </summary>
		public NewIssueFingerprint? Fingerprint { get; set; }

		/// <summary>
		/// Whether this event should be ignored
		/// </summary>
		public bool Ignored { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		public IssueEvent(ILogEvent stepEvent, ILogEventData stepEventData)
		{
			Event = stepEvent;
			EventData = stepEventData;
		}

		/// <summary>
		/// Tests whether this is a systemic event id
		/// </summary>
		/// <returns>True if this is a systemic event id</returns>
		public bool IsSystemic()
		{
			return EventId.HasValue && (EventId.Value.Id >= KnownLogEvents.Systemic.Id && EventId.Value.Id <= KnownLogEvents.Systemic_Max.Id);
		}

		/// <inheritdoc/>
		public override string ToString()
		{
			return $"[{Event.LineIndex}] {EventData.Message}";
		}
	}

	/// <summary>
	/// A group of <see cref="IssueEvent"/> objects with their fingerprint
	/// </summary>
	class IssueEventGroup
	{
		/// <summary>
		/// Digest of the fingerprint, for log tracking
		/// </summary>
		public Md5Hash Digest { get; }

		/// <summary>
		/// Fingerprint for the event
		/// </summary>
		public NewIssueFingerprint Fingerprint { get; }

		/// <summary>
		/// Individual log events
		/// </summary>
		public List<IssueEvent> Events { get; } = new List<IssueEvent>();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="fingerprint">Fingerprint for the event</param>
		public IssueEventGroup(NewIssueFingerprint fingerprint)
		{
			Digest = Md5Hash.Compute(Encoding.UTF8.GetBytes(fingerprint.ToString()));
			Fingerprint = fingerprint;
		}

		/// <summary>
		/// Merge with another group
		/// </summary>
		/// <param name="otherGroup">The group to merge with</param>
		/// <returns>A new group combining both groups</returns>
		public IssueEventGroup MergeWith(IssueEventGroup otherGroup)
		{
			IssueEventGroup newGroup = new IssueEventGroup(NewIssueFingerprint.Merge(Fingerprint, otherGroup.Fingerprint));
			newGroup.Events.AddRange(Events);
			newGroup.Events.AddRange(otherGroup.Events);
			return newGroup;
		}

		/// <inheritdoc/>
		public override string ToString() => Fingerprint.ToString();
	}
}
