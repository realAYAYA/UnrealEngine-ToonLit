// Copyright Epic Games, Inc. All Rights Reserved.

using System.Text.RegularExpressions;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

#nullable enable

namespace AutomationUtils.Matchers
{
	/// <summary>
	/// Matcher for engine crashes
	/// </summary>
	class CrashEventMatcher : ILogEventMatcher
	{
		static readonly Regex s_exitCodePattern = new Regex(@"ExitCode=(3|139|255)(?!\d)");

		static readonly Regex s_appErrorPattern = new Regex(@"^\s*[A-Za-z]+: Error: appError called: ");

		static readonly Regex s_assertionFailedPattern = new Regex(@"^Assertion failed: ");

		public LogEventMatch? Match(ILogCursor cursor)
		{
			if (cursor.Contains("begin: stack for UAT"))
			{
				for (int maxOffset = 1; maxOffset < 100; maxOffset++)
				{
					if (cursor.Contains(maxOffset, "end: stack for UAT"))
					{
						LogEventBuilder builder = new LogEventBuilder(cursor, lineCount: maxOffset + 1);
						return builder.ToMatch(LogEventPriority.BelowNormal, GetLogLevel(cursor), KnownLogEvents.Engine_Crash);
					}
				}
			}
			if (cursor.IsMatch(s_appErrorPattern))
			{
				LogEventBuilder builder = new LogEventBuilder(cursor);
				return builder.ToMatch(LogEventPriority.Normal, LogLevel.Error, KnownLogEvents.Engine_AppError);
			}
			if (cursor.IsMatch(s_assertionFailedPattern))
			{
				LogEventBuilder builder = new LogEventBuilder(cursor);
				return builder.ToMatch(LogEventPriority.Normal, LogLevel.Error, KnownLogEvents.Engine_AssertionFailed);
			}
			if (cursor.Contains("AutomationTool: Stack:"))
			{
				LogEventBuilder builder = new LogEventBuilder(cursor);
				while (builder.Current.Contains(1, "AutomationTool: Stack:"))
				{
					builder.MoveNext();
				}
				return builder.ToMatch(LogEventPriority.Low, LogLevel.Error, KnownLogEvents.AutomationTool_Crash);
			}

			Match? match;
			if (cursor.TryMatch(s_exitCodePattern, out match))
			{
				LogEventBuilder builder = new LogEventBuilder(cursor);
				builder.Annotate("exitCode", match.Groups[1]);
				return builder.ToMatch(LogEventPriority.Low, LogLevel.Error, KnownLogEvents.AutomationTool_CrashExitCode);
			}
			return null;
		}

		static readonly Regex s_errorPattern = new Regex("[Ee]rror:");
		static readonly Regex s_warningPattern = new Regex("[Ww]arning:");

		static LogLevel GetLogLevel(ILogCursor cursor)
		{
			if(cursor.IsMatch(0, s_errorPattern))
			{
				return LogLevel.Error;
			}
			else if(cursor.IsMatch(0, s_warningPattern))
			{
				return LogLevel.Warning;
			}
			else
			{
				return LogLevel.Information;
			}
		}
	}
}
