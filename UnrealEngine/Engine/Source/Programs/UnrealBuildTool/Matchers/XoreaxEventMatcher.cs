// Copyright Epic Games, Inc. All Rights Reserved.

using System.Text.RegularExpressions;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool.Matchers
{
	class XoreaxEventMatcher : ILogEventMatcher
	{
		static readonly Regex s_buildService = new Regex(@"\(BuildService.exe\) is not running");

		static readonly Regex s_xgConsole = new Regex(@"BUILD FAILED: (.*)xgConsole\.exe(.*)");

		static readonly Regex s_buildSystemWarning = new Regex(@"^\s*--------------------Build System Warning[- ]");
		static readonly Regex s_buildSystemWarningNext = new Regex(@"^(\s*)([^ ].*):");

		static readonly Regex s_cacheWarning = new Regex(@"^\s*WARNING: \d+ items \([^\)]*\) removed from the cache due to reaching the cache size limit");

		public LogEventMatch? Match(ILogCursor cursor)
		{
			if (cursor.IsMatch(s_cacheWarning))
			{
				LogEventBuilder builder = new LogEventBuilder(cursor);
				return builder.ToMatch(LogEventPriority.High, LogLevel.Information, KnownLogEvents.Systemic_Xge_CacheLimit);
			}

			if (cursor.IsMatch(s_buildService))
			{
				LogEventBuilder builder = new LogEventBuilder(cursor);
				return builder.ToMatch(LogEventPriority.High, LogLevel.Information, KnownLogEvents.Systemic_Xge_ServiceNotRunning);
			}

			if (cursor.IsMatch(s_xgConsole))
			{
				LogEventBuilder builder = new LogEventBuilder(cursor);
				return builder.ToMatch(LogEventPriority.High, LogLevel.Error, KnownLogEvents.Systemic_Xge_BuildFailed);
			}

			if (cursor.IsMatch(s_buildSystemWarning))
			{
				LogEventBuilder builder = new LogEventBuilder(cursor);
				if (builder.Next.TryMatch(s_buildSystemWarningNext, out Match? prefix))
				{
					builder.MoveNext();

					string message = prefix.Groups[2].Value;

					EventId eventId = KnownLogEvents.Systemic_Xge;
					if (Regex.IsMatch(message, "Failed to connect to Coordinator"))
					{
						eventId = KnownLogEvents.Systemic_Xge_Standalone;
					}

					Regex prefixPattern = new Regex($"^{Regex.Escape(prefix.Groups[1].Value)}\\s");
					while (builder.Next.IsMatch(prefixPattern))
					{
						builder.MoveNext();
					}

					return builder.ToMatch(LogEventPriority.High, LogLevel.Information, eventId);
				}
			}
			return null;
		}
	}
}
