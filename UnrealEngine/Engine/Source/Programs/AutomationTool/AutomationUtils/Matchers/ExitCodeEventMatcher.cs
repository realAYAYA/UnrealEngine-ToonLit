// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using Microsoft.Extensions.Logging;
using System.Text.RegularExpressions;

#nullable enable

namespace AutomationUtils.Matchers
{
	/// <summary>
	/// Matcher for editor/UAT instances exiting with an error
	/// </summary>
	class ExitCodeEventMatcher : ILogEventMatcher
	{
		static readonly Regex s_pattern = new Regex(
			@"Editor terminated with exit code -?[1-9]|(Error executing.+)(tool returned code)(.+)");

		public LogEventMatch? Match(ILogCursor cursor)
		{
			int numLines = 0;
			for (; ; )
			{
				if (cursor.IsMatch(numLines, s_pattern))
				{
					numLines++;
				}
				else
				{
					break;
				}
			}

			if (numLines > 0)
			{
				LogEventBuilder builder = new LogEventBuilder(cursor);
				builder.MoveNext(numLines - 1);
				return builder.ToMatch(LogEventPriority.Low, LogLevel.Error, KnownLogEvents.ExitCode);
			}
			return null;
		}
	}
}

