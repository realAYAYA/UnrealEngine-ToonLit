// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using Microsoft.Extensions.Logging;
using System.Text.RegularExpressions;

#nullable enable

namespace AutomationUtils.Matchers
{
	class SystemicEventMatcher : ILogEventMatcher
	{
		static readonly Regex s_ddc = new Regex(@"^\s*LogDerivedDataCache: Warning:");

		static readonly Regex s_pdbUtil = new Regex(@"^\s*ERROR: Error: EC_OK");
		static readonly Regex s_pdbUtilSuffix = new Regex(@"^\s*ERROR:\s*$");

		static readonly Regex s_roboMerge = new Regex(@"RoboMerge\/gates.*already locked on Commit Server by buildmachine");

		static readonly Regex s_hostDown = new Regex(@"ERROR: System\.IO\.IOException: Host is down");

		static readonly Regex s_missingFileList = new Regex(@"^\s*ERROR: Missing local or shared file list.*\.xml");

		public LogEventMatch? Match(ILogCursor cursor)
		{
			if (cursor.IsMatch(s_ddc))
			{
				return new LogEventBuilder(cursor).ToMatch(LogEventPriority.High, LogLevel.Information, KnownLogEvents.Systemic_SlowDDC);
			}
			if (cursor.IsMatch(s_pdbUtil))
			{
				LogEventBuilder builder = new LogEventBuilder(cursor);
				while (builder.Next.IsMatch(s_pdbUtilSuffix))
				{
					builder.MoveNext();
				}
				return builder.ToMatch(LogEventPriority.High, LogLevel.Information, KnownLogEvents.Systemic_PdbUtil);
			}
			if (cursor.IsMatch(s_roboMerge))
			{
				return new LogEventBuilder(cursor).ToMatch(LogEventPriority.Low, LogLevel.Information, KnownLogEvents.Systemic_RoboMergeGateLocked);
			}
			if (cursor.IsMatch(s_hostDown))
			{
				return new LogEventBuilder(cursor).ToMatch(LogEventPriority.Low, LogLevel.Information, KnownLogEvents.Systemic_HostDownIOException);
			}
			if (cursor.Contains("LogXGEController: Warning: XGEControlWorker.exe does not exist"))
			{
				return new LogEventBuilder(cursor).ToMatch(LogEventPriority.High, LogLevel.Information, KnownLogEvents.Systemic_MissingXgeControlWorker);
			}

			if (cursor.IsMatch(s_missingFileList))
			{
				return new LogEventBuilder(cursor).ToMatch(LogEventPriority.High, LogLevel.Error, KnownLogEvents.Systemic_MissingFileList);
			}

			if (cursor.Contains("SignTool Error: The specified timestamp server either could not be reached or"))
			{
				return new LogEventBuilder(cursor).ToMatch(LogEventPriority.Normal, LogLevel.Information, KnownLogEvents.Systemic_SignToolTimeStampServer);
			}
			else if (cursor.Contains("SignTool Error: An error occurred while attempting to sign"))
			{
				return new LogEventBuilder(cursor).ToMatch(LogEventPriority.Normal, LogLevel.Information, KnownLogEvents.Systemic_SignTool);
			}

			return null;
		}
	}
}
