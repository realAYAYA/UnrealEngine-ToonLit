// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Issues.Handlers
{
	/// <summary>
	/// Instance of a particular shader compile error
	/// </summary>
	[IssueHandler(Priority = 10)]
	public class ShaderIssueHandler : IssueHandler
	{
		readonly List<IssueEventGroup> _issues = new List<IssueEventGroup>();

		static bool IsMatchingEventId(EventId id) => id == KnownLogEvents.Engine_ShaderCompiler;
		static bool IsMaskedEventId(EventId id) => id == KnownLogEvents.Generic || id == KnownLogEvents.ExitCode || id == KnownLogEvents.Systemic_Xge_BuildFailed || id == KnownLogEvents.Engine_Crash || id == KnownLogEvents.AutomationTool_CrashExitCode || id == KnownLogEvents.Engine_AppError;

		/// <inheritdoc/>
		public override bool HandleEvent(IssueEvent logEvent)
		{
			if (logEvent.EventId.HasValue)
			{
				EventId eventId = logEvent.EventId.Value;
				if (IsMatchingEventId(eventId))
				{
					IssueEventGroup issue = new IssueEventGroup("Shader", "Shader compile {Severity} in {Files}", IssueChangeFilter.Code);
					issue.Events.Add(logEvent);
					issue.Keys.AddSourceFiles(logEvent);
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
