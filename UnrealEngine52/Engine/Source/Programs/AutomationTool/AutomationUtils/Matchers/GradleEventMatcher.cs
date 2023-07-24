// Copyright Epic Games, Inc. All Rights Reserved.

using System.Text.RegularExpressions;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

#nullable enable

namespace AutomationUtils.Matchers
{
	/// <summary>
	/// Matcher for Gradle errors
	/// </summary>
	class GradleEventMatcher : ILogEventMatcher
	{
		static readonly Regex s_failurePattern = new Regex(@"^\s*FAILURE:");
		static readonly Regex s_blankPattern = new Regex(@"^\s*$");
		static readonly Regex s_indentPattern = new Regex(@"^(\s*)\*");

		public LogEventMatch? Match(ILogCursor cursor)
		{
			if (cursor.IsMatch(s_failurePattern))
			{
				int maxOffset = 1;
				for (; ; )
				{
					int newMaxOffset = maxOffset;
					while (cursor.IsMatch(newMaxOffset, s_blankPattern))
					{
						newMaxOffset++;
					}

					Match? match;
					if (!cursor.TryMatch(newMaxOffset, s_indentPattern, out match))
					{
						break;
					}
					maxOffset = newMaxOffset + 1;

					while (cursor.IsMatch(maxOffset, new Regex($"^{match.Groups[1].Value}")))
					{
						maxOffset++;
					}
				}

				LogEventBuilder builder = new LogEventBuilder(cursor, maxOffset);
				return builder.ToMatch(LogEventPriority.Normal, LogLevel.Error, KnownLogEvents.AutomationTool);
			}

			return null;
		}
	}
}

