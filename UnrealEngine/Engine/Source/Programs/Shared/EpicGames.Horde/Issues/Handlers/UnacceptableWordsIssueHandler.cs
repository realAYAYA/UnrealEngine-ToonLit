// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using EpicGames.Core;

namespace EpicGames.Horde.Issues.Handlers
{
	/// <summary>
	/// Instance of a Perforce case mismatch error
	/// </summary>
	[IssueHandler(Priority = 8)]
	public class UnacceptableWordsIssueHandler : IssueHandler
	{
		readonly List<IssueEventGroup> _issues = new List<IssueEventGroup>();

		/// <inheritdoc/>
		public override bool HandleEvent(IssueEvent issueEvent)
		{
			if (issueEvent.EventId == KnownLogEvents.AutomationTool_UnacceptableWords)
			{
				IssueEventGroup issue = new IssueEventGroup("UnacceptableWords", "Unacceptable words in {Files}", IssueChangeFilter.Code);
				issue.Events.Add(issueEvent);
				issue.Keys.AddSourceFiles(issueEvent);
				_issues.Add(issue);

				return true;
			}
			return false;
		}

		/// <inheritdoc/>
		public override IEnumerable<IssueEventGroup> GetIssues() => _issues;
	}
}
