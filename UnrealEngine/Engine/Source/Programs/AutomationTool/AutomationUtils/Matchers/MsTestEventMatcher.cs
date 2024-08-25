// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using Microsoft.Extensions.Logging;
using System.Text.RegularExpressions;

#nullable enable

namespace AutomationUtils.Matchers
{
	class MsTestEventMatcher : ILogEventMatcher
	{
		static readonly Regex s_failedPattern = new Regex(@"^  \s*Failed [A-Za-z0-9_]+ \[\d+ (?:s|ms)\]$");
		static readonly Regex s_detailPattern = new Regex(@"^  \s*(?:Error Message|Stack Trace):$");

		public LogEventMatch? Match(ILogCursor cursor)
		{
			if (cursor.IsMatch(s_failedPattern))
			{
				int lineCount = 1;
				while (cursor.IsMatch(lineCount, s_detailPattern) || cursor.IsHanging(lineCount, cursor.CurrentLine))
				{
					lineCount++;
				}
				while (lineCount > 0 && cursor.IsBlank(lineCount - 1))
				{
					lineCount--;
				}

				if (lineCount > 1)
				{
					LogEventBuilder builder = new LogEventBuilder(cursor, lineCount);
					return builder.ToMatch(LogEventPriority.High, LogLevel.Error, KnownLogEvents.MSTest);
				}
			}
			return null;
		}
	}
}
