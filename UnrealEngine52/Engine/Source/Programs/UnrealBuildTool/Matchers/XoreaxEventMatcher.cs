// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics.CodeAnalysis;
using System.Text.RegularExpressions;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool.Matchers
{
	class XoreaxEventMatcher : ILogEventMatcher
	{
		public const string PropertyName = "name";
		public const string PropertyAgent = "agent";
		public const string PropertyDuration = "duration";
		public const string PropertyStartTime = "startTime";
		
		static readonly Regex s_buildService = new Regex(@"\(BuildService.exe\) is not running");

		static readonly Regex s_xgConsole = new Regex(@"BUILD FAILED: (.*)xgConsole\.exe(.*)");

		static readonly Regex s_buildSystemError = new Regex(@"^\s*--------------------Build System Error");
		static readonly Regex s_buildSystemErrorEnd = new Regex(@"^(\s*)--------------------");

		static readonly Regex s_buildSystemWarning = new Regex(@"^\s*--------------------Build System Warning[- ]");
		static readonly Regex s_buildSystemWarningNext = new Regex(@"^(\s*)([^ ].*):");

		static readonly Regex s_cacheWarning = new Regex(@"^\s*WARNING: \d+ items \([^\)]*\) removed from the cache due to reaching the cache size limit");
		static readonly Regex s_cacheWarning2 = new Regex(@"^\s*WARNING: Several items removed from the cache due to reaching the cache size limit");
		static readonly Regex s_cacheWarning3 = new Regex(@"^\s*WARNING: The Build Cache is close to full");
		
		static readonly Regex s_taskFinishedOnHelper = new Regex(@"(.*?) \(Agent '(.+)', (.*) at \+(.*)\)");
		static readonly Regex s_taskFinishedLocally = new Regex(@"(.*?) \((.*) at \+(.*)\)");
		static readonly Regex s_duration = new Regex(@"(\d+):(\d+)\.(\d+)");
		static readonly Regex s_startTime = new Regex(@"\+?(\d+):(\d+)");

		public LogEventMatch? Match(ILogCursor cursor)
		{
			if (cursor.IsMatch(s_cacheWarning) || cursor.IsMatch(s_cacheWarning2) || cursor.IsMatch(s_cacheWarning3))
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

			if (cursor.IsMatch(s_buildSystemError))
			{
				LogEventBuilder builder = new LogEventBuilder(cursor);
				builder.MoveNext();

				for(int idx = 0; idx < 100; idx++)
				{
					if (builder.Current.IsMatch(idx, s_buildSystemErrorEnd))
					{
						builder.MoveNext(idx);
						break;
					}
				}

				return builder.ToMatch(LogEventPriority.High, LogLevel.Error, KnownLogEvents.Systemic_Xge);
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
			
			if (cursor.TryMatch(s_taskFinishedOnHelper, out Match? taskHelperMatch))
			{
				LogEventBuilder builder = new LogEventBuilder(cursor);
				builder.AddProperty(PropertyName, taskHelperMatch.Groups[1].Value);
				builder.AddProperty(PropertyAgent, taskHelperMatch.Groups[2].Value);
				SetDuration(builder, taskHelperMatch.Groups[3].Value);
				SetStartTime(builder, taskHelperMatch.Groups[4].Value);
				return builder.ToMatch(LogEventPriority.High, LogLevel.Information, KnownLogEvents.Systemic_Xge_TaskMetadata);
			}
			
			if (cursor.TryMatch(s_taskFinishedLocally, out Match? taskLocalMatch))
			{
				LogEventBuilder builder = new LogEventBuilder(cursor);
				builder.AddProperty(PropertyName, taskLocalMatch.Groups[1].Value);
				SetDuration(builder, taskLocalMatch.Groups[2].Value);
				SetStartTime(builder, taskLocalMatch.Groups[3].Value);
				return builder.ToMatch(LogEventPriority.High, LogLevel.Information, KnownLogEvents.Systemic_Xge_TaskMetadata);
			}
			
			return null;
		}

		private static void SetDuration(LogEventBuilder builder, string durationText)
		{
			Match m = s_duration.Match(durationText);
			if (m.Success)
			{
				int minutes = Convert.ToInt32(m.Groups[1].Value);
				int seconds = Convert.ToInt32(m.Groups[2].Value);
				int milliseconds = Convert.ToInt32(m.Groups[3].Value) * 10;
				TimeSpan duration = new TimeSpan(0, 0, minutes, seconds, milliseconds);
				builder.AddProperty(PropertyDuration, (int)duration.TotalMilliseconds);
			}
		}
		
		private static void SetStartTime(LogEventBuilder builder, string startTimeText)
		{
			Match m = s_startTime.Match(startTimeText);
			if (m.Success)
			{
				int minutes = Convert.ToInt32(m.Groups[1].Value);
				int seconds = Convert.ToInt32(m.Groups[2].Value);
				TimeSpan startTime = new TimeSpan(0, minutes, seconds);
				builder.AddProperty(PropertyStartTime, (int)startTime.TotalMilliseconds);
			}
		}
	}
}
