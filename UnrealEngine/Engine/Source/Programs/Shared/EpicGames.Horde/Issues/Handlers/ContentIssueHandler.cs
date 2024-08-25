// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Issues.Handlers
{
	/// <summary>
	/// Instance of a particular compile error
	/// </summary>
	[IssueHandler(Priority = 10)]
	public class ContentIssueHandler : IssueHandler
	{
		readonly List<IssueEventGroup> _issues = new List<IssueEventGroup>();

		static bool IsMatchingEventId(EventId eventId) => eventId == KnownLogEvents.Engine_AssetLog;
		static bool IsMaskedEventId(EventId eventId) => eventId == KnownLogEvents.ExitCode;

		/// <inheritdoc/>
		public override bool HandleEvent(IssueEvent issueEvent)
		{
			if (issueEvent.EventId != null)
			{
				EventId eventId = issueEvent.EventId.Value;
				if (IsMatchingEventId(eventId))
				{
					IssueEventGroup issue = new IssueEventGroup("Content", "{Severity} in {Files}", IssueChangeFilter.Content);
					issue.Events.Add(issueEvent);
					issue.Keys.AddAssets(issueEvent);
					_issues.Add(issue);

					return true;
				}
				else if (_issues.Count > 0 && IsMaskedEventId(eventId))
				{
					return true;
				}
			}
			return false;
		}

		/// <inheritdoc/>
		public override IEnumerable<IssueEventGroup> GetIssues() => _issues;
	}
}
