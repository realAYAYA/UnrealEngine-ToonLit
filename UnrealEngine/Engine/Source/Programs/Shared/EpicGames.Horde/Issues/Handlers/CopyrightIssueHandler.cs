// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using EpicGames.Core;

namespace EpicGames.Horde.Issues.Handlers
{
	/// <summary>
	/// Instance of a particular compile error
	/// </summary>
	[IssueHandler(Priority = 10)]
	public class CopyrightIssueHandler : IssueHandler
	{
		readonly List<IssueEventGroup> _issues = new List<IssueEventGroup>();

		/// <inheritdoc/>
		public override bool HandleEvent(IssueEvent issueEvent)
		{
			if (issueEvent.EventId == KnownLogEvents.AutomationTool_MissingCopyright)
			{
				IssueEventGroup issue = new IssueEventGroup("Copyright", "Missing copyright notice in {Files}", IssueChangeFilter.Code);
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
