// Copyright Epic Games, Inc. All Rights Reserved.

using System.Text.RegularExpressions;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

#nullable enable

namespace AutomationUtils.Matchers
{
	/// <summary>
	/// Matcher for docker errors
	/// </summary>
	class DockerEventMatcher : ILogEventMatcher
	{
		static readonly Regex s_noSpacePattern = new Regex(": no space left on device$");

		public LogEventMatch? Match(ILogCursor cursor)
		{
			if (cursor.IsMatch(s_noSpacePattern))
			{
				LogEventBuilder builder = new LogEventBuilder(cursor);
				return builder.ToMatch(LogEventPriority.Normal, LogLevel.Error, KnownLogEvents.Systemic_OutOfDiskSpace);
			}
			return null;
		}
	}
}
