// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;

namespace EpicGames.Horde.Issues.Handlers
{
	/// <summary>
	/// Default handler for log events not matched by any other handler
	/// </summary>
	[IssueHandler(Priority = 0)]
	public class DefaultIssueHandler : IssueHandler
	{
		readonly IssueHandlerContext _context;
		IssueEventGroup? _issue;

		/// <summary>
		/// Constructor
		/// </summary>
		public DefaultIssueHandler(IssueHandlerContext context) => _context = context;

		/// <inheritdoc/>
		public override bool HandleEvent(IssueEvent issueEvent)
		{
			if (_issue == null)
			{
				_issue = new IssueEventGroup("Default", "{Severity} in {Meta:Node}", IssueChangeFilter.All);
				_issue.Metadata.Add("Node", _context.NodeName);
				_issue.Keys.Add(IssueKey.FromStep(_context.StreamId, _context.TemplateId, _context.NodeName));
			}

			_issue.Events.Add(issueEvent);
			return true;
		}

		/// <inheritdoc/>
		public override IEnumerable<IssueEventGroup> GetIssues()
		{
			if (_issue != null)
			{
				yield return _issue;
			}
		}
	}
}
