// Copyright Epic Games, Inc. All Rights Reserved.

using System.Reflection;
using System.Text.RegularExpressions;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Parser
{
	/// <summary>
	/// Turns raw text output into structured logging events
	/// </summary>
	public class LogParser : LogEventParser
	{
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="logger">The logger to receive parsed output messages</param>
		/// <param name="ignorePatterns">List of patterns to ignore</param>
		/// <param name="logEventSinks">Additional sinks to receive log events</param>
		public LogParser(ILogger logger, List<string> ignorePatterns, List<ILogEventSink>? logEventSinks = null)
			: base(logger, logEventSinks)
		{
			foreach (string ignorePattern in ignorePatterns)
			{
				IgnorePatterns.Add(new Regex(ignorePattern, RegexOptions.Compiled));
			}

			foreach (Type type in Assembly.GetExecutingAssembly().GetTypes())
			{
				if (type.IsClass && typeof(ILogEventMatcher).IsAssignableFrom(type))
				{
					ILogEventMatcher matcher = (ILogEventMatcher)Activator.CreateInstance(type)!;
					Matchers.Add(matcher);
				}
			}
		}

		/// <summary>
		/// Static constructor
		/// </summary>
		static LogParser()
		{
			System.Text.RegularExpressions.Regex.CacheSize = 1000;
		}
	}
}
