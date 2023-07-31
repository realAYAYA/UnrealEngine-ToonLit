// Copyright Epic Games, Inc. All Rights Reserved.

using System.Text.RegularExpressions;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

#nullable enable

namespace AutomationUtils.Matchers
{
	/// <summary>
	/// Matcher for Gauntlet unit tests
	/// </summary>
	class GauntletEventMatcher : ILogEventMatcher
	{
		static readonly Regex s_engineTestPattern = new Regex(@"^(?<indent>\s*)Error: EngineTest.RunTests Group:(?<group>[^\s]+) \(");

		static readonly Regex s_mdHeadingPattern = new Regex(@"^\s*#{1,3} ");
		static readonly Regex s_mdTestNamePattern = new Regex(@"^\s*#####\s+(?<friendly_name>.*):\s*(?<name>\S+)\s*");
		static readonly Regex s_mdTestListPattern = new Regex(@"^\s*### The following tests failed:");

		static readonly Regex s_screenshotPattern = new Regex(@"Error: Screenshot '(?<screenshot>[^']+)' test failed");

		static readonly Regex s_genericPattern = new Regex("\\[ERROR\\] (.*)$");

		public LogEventMatch? Match(ILogCursor cursor)
		{
			Match? match;
			if(cursor.TryMatch(s_engineTestPattern, out match))
			{
				string indent = match.Groups["indent"].Value + " ";

				LogEventBuilder builder = new LogEventBuilder(cursor);
				builder.Annotate(match.Groups["group"]);

				string group = match.Groups["group"].Value;

				bool inErrorList = false;
				//				List<LogEventBuilder> ChildEvents = new List<LogEventBuilder>();

				//				int LineCount = 1;
				EventId eventId = KnownLogEvents.Gauntlet;
				while (builder.Next.CurrentLine != null && builder.IsNextLineAligned())
				{
					builder.MoveNext();
					if (inErrorList)
					{
						Match? testNameMatch;
						if (builder.Current.IsMatch(s_mdHeadingPattern))
						{
							inErrorList = false;
						}
						else if (builder.Current.TryMatch(s_mdTestNamePattern, out testNameMatch))
						{
							builder.AddProperty("group", group);//.Annotate().AddProperty("group", Group);

							builder.Annotate(testNameMatch.Groups["name"]);
							builder.Annotate(testNameMatch.Groups["friendly_name"]);

							eventId = KnownLogEvents.Gauntlet_UnitTest;//							ChildEvents.Add(ChildEventBuilder);
						}
					}
					else
					{
						if (builder.Current.IsMatch(s_mdTestListPattern))
						{
							inErrorList = true;
						}
					}
				}

				return builder.ToMatch(LogEventPriority.High, LogLevel.Error, eventId);
			}

			if (cursor.TryMatch(s_screenshotPattern, out match))
			{
				LogEventBuilder builder = new LogEventBuilder(cursor);
				builder.Annotate(match.Groups["screenshot"], LogEventMarkup.ScreenshotTest);//.MarkAsScreenshotTest();
				return builder.ToMatch(LogEventPriority.High, LogLevel.Error, KnownLogEvents.Gauntlet_ScreenshotTest);
			}

			if (cursor.TryMatch(s_genericPattern, out _))
			{
				LogEventBuilder builder = new LogEventBuilder(cursor);
				return builder.ToMatch(LogEventPriority.High, LogLevel.Error, KnownLogEvents.Generic);
			}

			return null;
		}
	}
}
