// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Linq;
using System.Text;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using Logging = Microsoft.Extensions.Logging;

namespace Gauntlet
{
	/// <summary>
	/// Represents a prepared summary of the most relevant info in a log. Generated once by a
	/// LogParser and cached.
	/// </summary>
	public class UnrealLog
	{
		/// <summary>
		/// Represents the level
		/// </summary>
		public enum LogLevel
		{
			Log,
			Display,
			Verbose,
			VeryVerbose,
			Warning,
			Error,
			Fatal
		}

		/// <summary>
		/// Set of log channels that are used to monitor Editor processing
		/// </summary>
		public static string[] EditorBusyChannels = new string[] { "Automation", "FunctionalTest", "Material", "DerivedDataCache", "ShaderCompilers", "Texture", "SkeletalMesh", "StaticMesh", "Python" };

		/// <summary>
		/// Represents an entry in an Unreal logfile with and contails the associated category, level, and message
		/// </summary>
		public class LogEntry
		{
			public string Prefix { get; private set; }

			/// <summary>
			/// Category of the entry. E.g for "LogNet" this will be "Net"
			/// </summary>
			public string Category { get; private set; }

			/// <summary>
			/// Represents the level of the entry
			/// </summary>
			public LogLevel Level { get; private set; }

			/// <summary>
			/// The message string from the entry
			/// </summary>
			public string Message { get; private set; }

			/// <summary>
			/// Format the entry as it would have appeared in the log. 
			/// </summary>
			/// <returns></returns>
			public override string ToString()
			{
				// LogFoo: Display: Some Message
				// Match how Unreal does not display the level for 'Log' level messages
				if (Level == LogLevel.Log)
				{
					return string.Format("{0}{1}: {2}", Prefix, Category, Message);
				}
				else
				{
					return string.Format("{0}{1}: {2}: {3}", Prefix, Category, Level, Message);
				}
			}

			/// <summary>
			/// Constructor that requires all info
			/// </summary>
			/// <param name="InCategory"></param>
			/// <param name="InLevel"></param>
			/// <param name="InMessage"></param>
			public LogEntry(string InPrefix, string InCategory, LogLevel InLevel, string InMessage)
			{
				Prefix = InPrefix;
				Category = InCategory;
				Level = InLevel;
				Message = InMessage;
			}
		}

		/// <summary>
		/// Compound object that represents a fatal entry 
		/// </summary>
		public class CallstackMessage
		{
			public int Position;
			public string Message;
			public string[] Callstack;
			public bool IsEnsure;
			public bool IsPostMortem;

			/// <summary>
			/// Generate a string that represents a CallstackMessage formatted to be inserted into a log file.
			/// </summary>
			/// <returns>Formatted log string version of callstack.</returns>
			public string FormatForLog()
			{
				string FormattedString = string.Format("{0}\n", Message);
				foreach (string StackLine in Callstack)
				{
					FormattedString += string.Format("\t{0}\n", StackLine);
				}
				return FormattedString;
			}
		};

		/// <summary>
		/// Information about the current platform that will exist and be extracted from the log
		/// </summary>
		public class PlatformInfo
		{
			public string OSName;
			public string OSVersion;
			public string CPUName;
			public string GPUName;
		}

		/// <summary>
		/// Information about the current build info
		/// </summary>
		public class BuildInfo
		{
			public string BuildVersion;
			public string BranchName;
			public int Changelist;
		}

		/// <summary>
		/// Build info from the log
		/// </summary>
		public BuildInfo LoggedBuildInfo;

		/// <summary>
		/// Platform info from the log
		/// </summary>
		public PlatformInfo LoggedPlatformInfo;

		/// <summary>
		/// Entries in the log
		/// </summary>
		public IEnumerable<LogEntry> LogEntries = Enumerable.Empty<LogEntry>();

		/// <summary>
		/// Warnings for this role
		/// </summary>
		public IEnumerable<LogEntry> Warnings { get { return LogEntries.Where(E => E.Level == LogLevel.Warning); } }

		/// <summary>
		/// Errors for this role
		/// </summary>
		public IEnumerable<LogEntry> Errors { get { return LogEntries.Where(E => E.Level == LogLevel.Error); } }

		/// <summary>
		/// Fatal error instance if one occurred
		/// </summary>
		public CallstackMessage FatalError;

		/// <summary>
		/// A list of ensures if any occurred
		/// </summary>
		public IEnumerable<CallstackMessage> Ensures = Enumerable.Empty<CallstackMessage>();

		/// <summary>
		/// Number of lines in the log
		/// </summary>
		public int LineCount;

		/// <summary>
		/// True if the engine reached initialization
		/// </summary>
		public bool EngineInitialized;

		/// <summary>
		/// True if the instance requested exit
		/// </summary>
		public bool RequestedExit;
		public string RequestedExitReason;
		public bool HasTestExitCode;
		public int TestExitCode;

		// Temp for migrating some code
		public string FullLogContent;

		/// <summary>
		/// Returns true if this log indicates the Unreal instance exited abnormally
		/// </summary>
		public bool HasAbnormalExit
		{
			get
			{
				return FatalError != null
					|| EngineInitialized == false
					|| (RequestedExit == false && HasTestExitCode == false);
			}
		}
	}

	/// <summary>
	/// Parse Unreal log from string chunk and aggregate lines as LogEvents
	/// Support structure logging output and legacy logging style
	/// </summary>
	public class UnrealLogStreamParser
	{

		static class LogEventProperties
		{
			public static readonly string Channel = "_channel";
			public static readonly string Severity = "_severity";
		}

		protected List<LogEvent> LogEvents { get; private set; }

		private List<string> UnidentifiedLogLevels { get; set; }

		public UnrealLogStreamParser()
		{
			LogEvents = new List<LogEvent>();
			UnidentifiedLogLevels = new List<string>();
		}

		/// <summary>
		/// Clear aggregated log events 
		/// </summary>
		public void Clear()
		{
			LogEvents.Clear();
		}

		/// <summary>
		/// Parse a string as log and aggregate identified unreal log lines
		/// </summary>
		/// <param name="InContent"></param>
		/// <param name="LineOffset">Line offset to start parsing and aggregate</param>
		/// <returns>The number of line parsed</returns>
		public int ReadStream(string InContent, int LineOffset = 0)
		{
			int Cursor = 0;

			for (int BaseIdx = 0; BaseIdx < InContent.Length;)
			{
				// Extract the next line
				int EndIdx = InContent.IndexOf('\n', BaseIdx);
				if (EndIdx == -1)
				{
					break;
				}

				// Position Cursor
				if (Cursor < LineOffset)
				{
					// Skip to next line
					Cursor++;
					BaseIdx = EndIdx + 1;
					continue;
				}

				// Skip over any windows CR-LF line endings
				int LineEndIdx = EndIdx;
				if (LineEndIdx > BaseIdx && InContent[LineEndIdx - 1] == '\r')
				{
					LineEndIdx--;
				}

				// Grab a line
				string Line = InContent.Substring(BaseIdx, LineEndIdx - BaseIdx);
				// Move to the next line
				Cursor++;
				BaseIdx = EndIdx + 1;

				// Parse the line
				if (Line.Length > 0 && Line[0] == '{')
				{

					try
					{
						byte[] Buffer = Encoding.UTF8.GetBytes(Line);
						LogEvent LogEntry = LogEvent.Read(JsonLogEvent.Parse(Buffer).Data.Span);
						LogEvents.Add(LogEntry);
						continue;
					}
					catch { /* Ignore the exception and handle the line like a regular log line then */ }
				}

				// Parse the line as Unreal legacy line
				Match MatchLine = Regex.Match(Line, @"(?<channel>[A-Za-z][\w\d]+):\s(?:(?<level>Display|Verbose|VeryVerbose|Warning|Error|Fatal):\s)?(?<message>.*)");

				if (MatchLine.Success)
				{
					string Channel = MatchLine.Groups["channel"].ToString();
					string LevelStr = MatchLine.Groups["level"].ToString();
					string Message = MatchLine.Groups["message"].ToString();

					Logging.LogLevel Level = Logging.LogLevel.Information;
					switch (LevelStr)
					{
						case "Display":
							Level = Logging.LogLevel.Information;
							break;

						case "Verbose":
						case "VeryVerbose":
							Level = Logging.LogLevel.Debug;
							break;

						default:
							if (!string.IsNullOrEmpty(LevelStr))
							{
								if (!Enum.TryParse(LevelStr, out Level))
								{
									// only show a warning once
									if (!UnidentifiedLogLevels.Contains(LevelStr))
									{
										UnidentifiedLogLevels.Add(LevelStr);
										Log.Warning("Failed to match log level {0} to enum!", LevelStr);
									}
								}
							}
							break;
					}

					Dictionary<string, object> Properties = new Dictionary<string, object>();
					string Format = Message;
					if (!string.IsNullOrEmpty(LevelStr))
					{
						Properties.Add(LogEventProperties.Severity, new LogValue(LogValueType.Severity, LevelStr));
						Format = $"{{{LogEventProperties.Severity}}}: {Format}";
					}
					Properties.Add(LogEventProperties.Channel, new LogValue(LogValueType.Channel, Channel));
					Format = $"{{{LogEventProperties.Channel}}}: {Format}";

					LogEvents.Add(new LogEvent(new DateTime(0), Level, KnownLogEvents.Engine_LogChannel, 0, 1, MatchLine.Groups[0].ToString(), Format, Properties, null));
				}
				else
				{
					// Not an Unreal Engine log line
					LogEvents.Add(new LogEvent(new DateTime(0), Logging.LogLevel.Information, new EventId(0), 0, 1, Line, Line, null, null));
				}
			}
			return Cursor - LineOffset;
		}

		/// <summary>
		/// Return All the LogEvent instances
		/// </summary>
		/// <returns></returns>
		public IEnumerable<LogEvent> GetEvents()
		{
			return LogEvents;
		}

		/// <summary>
		/// Return the LogEvent instances which property type and value match requirements
		/// </summary>
		/// <param name="PropertyName">The property name type to match the value</param>
		/// <param name="InValues">The property values to match</param>
		/// <param name="ExactMatch">Whether to use an exact match or partial match</param>
		/// <returns></returns>
		public IEnumerable<LogEvent> GetEventsFromProperty(string PropertyName, IEnumerable<string> InValues, bool ExactMatch = false)
		{
			IEnumerable<LogEvent> Events;

			if (ExactMatch)
			{
				Events = LogEvents.Where(E =>
				{
					LogValue PropertyValue = null;
					if (E.TryGetProperty(PropertyName, out PropertyValue))
					{
						return InValues.Contains(PropertyValue.Text, StringComparer.OrdinalIgnoreCase);
					}

					return false;

				});
			}
			else
			{
				// partial match
				Events = LogEvents.Where(E =>
				{
					LogValue PropertyValue = null;
					if (!E.TryGetProperty(PropertyName, out PropertyValue))
					{
						return false;
					}

					foreach (string Value in InValues)
					{
						if (PropertyValue.Text.IndexOf(Value, StringComparison.OrdinalIgnoreCase) >= 0)
						{
							return true;
						}
					}

					return false;
				});
			}
			return Events;
		}

		/// <summary>
		/// Return the LogEvent instances which property value meet predicate
		/// </summary>
		/// <param name="PropertyName"></param>
		/// <param name="InPredicate"></param>
		/// <returns></returns>
		public IEnumerable<LogEvent> GetEventsFromProperty(string PropertyName, Predicate<string> InPredicate)
		{
			IEnumerable<LogEvent> Events;

			Events = LogEvents.Where(E =>
			{
				LogValue PropertyValue = null;
				if (E.TryGetProperty(PropertyName, out PropertyValue))
				{
					return InPredicate(PropertyValue.Text);
				}

				return false;
			});

			return Events;
		}

		/// <summary>
		/// Return the LogEvent instances that match the channel names
		/// </summary>
		/// <param name="Channels">The names of the channel to match</param>
		/// <param name="ExactMatch"></param>
		/// <returns></returns>
		public IEnumerable<LogEvent> GetEventsFromChannels(IEnumerable<string> Channels, bool ExactMatch = true)
		{
			return GetEventsFromProperty(LogEventProperties.Channel, Channels, ExactMatch);
		}

		/// <summary>
		/// Return the LogEvent instances which Channel property meet predicate
		/// </summary>
		/// <param name="InPredicate"></param>
		/// <returns></returns>
		public IEnumerable<LogEvent> GetEventsFromChannels(Predicate<string> InPredicate)
		{
			return GetEventsFromProperty(LogEventProperties.Channel, InPredicate);
		}

		/// <summary>
		/// Return the LogEvent instances that match the Editor busy channels
		/// </summary>
		/// <returns></returns>
		public IEnumerable<LogEvent> GetEventsFromEditorBusyChannels()
		{
			return GetEventsFromChannels(UnrealLog.EditorBusyChannels, false);
		}

		/// <summary>
		/// Return the log lines that match the channel names
		/// </summary>
		/// <param name="Channels">The names of the channel to match</param>
		/// <param name="ExactMatch"></param>
		/// <returns></returns>
		public IEnumerable<string> GetLogFromChannels(IEnumerable<string> Channels, bool ExactMatch = true)
		{
			return GetEventsFromChannels(Channels, ExactMatch).Select(E => E.Message);
		}

		/// <summary>
		/// Return the log lines that match the channel names ignoring the "log" prefix
		/// </summary>
		/// <param name="Channels"></param>
		/// <returns></returns>
		public IEnumerable<string> GetLogFromShortNameChannels(IEnumerable<string> Channels)
		{
			return GetEventsFromChannels(
				V => Channels.Contains(
					V.StartsWith("log", StringComparison.OrdinalIgnoreCase)? V.Substring(3) : V,
					StringComparer.OrdinalIgnoreCase
				)
			).Select(E => E.Message);
		}

		/// <summary>
		/// Return the log lines that match the Editor busy channels
		/// </summary>
		/// <returns></returns>
		public IEnumerable<string> GetLogFromEditorBusyChannels()
		{
			return GetLogFromChannels(UnrealLog.EditorBusyChannels, false);
		}

		/// <summary>
		/// Return the log lines that match the channel name
		/// </summary>
		/// <param name="Channel">The channel name to match</param>
		/// <param name="ExactMatch"></param>
		/// <returns></returns>
		public IEnumerable<string> GetLogFromChannel(string Channel, bool ExactMatch = true)
		{
			return GetLogFromChannels(new string[] { Channel }, ExactMatch);
		}

		/// <summary>
		/// Return the log lines that match the severity name
		/// </summary>
		/// <param name="Severity">The severity string to match</param>
		/// <returns></returns>
		public IEnumerable<string> GetLogFromSeverity(string Severity)
		{
			return GetEventsFromProperty(LogEventProperties.Severity, new string[] { Severity }, true).Select(E => E.Message);
		}

		/// <summary>
		/// Return all the aggregated log lines 
		/// </summary>
		/// <returns></returns>
		public IEnumerable<string> GetLogLines()
		{
			return LogEvents.Select(E => E.Message);
		}

		/// <summary>
		/// Return the log lines that contain a string
		/// </summary>
		/// <param name="Text">The text to match in the line</param>
		/// <returns></returns>
		public IEnumerable<string> GetLogLinesContaining(string Text)
		{
			return GetLogLines().Where(L => L.IndexOf(Text, StringComparison.OrdinalIgnoreCase) >= 0);
		}

		/// <summary>
		/// Return the log lines that match the regex pattern
		/// </summary>
		/// <param name="Pattern">The regex pattern to match in the line</param>
		/// <returns></returns>
		public IEnumerable<string> GetLogLinesMatchingPattern(string Pattern)
		{
			Regex RegexPattern = new Regex(Pattern, RegexOptions.IgnoreCase);
			return GetLogLines().Where(L => RegexPattern.IsMatch(L));
		}
	}

	/// <summary>
	/// Helper class for parsing logs
	/// </summary>
	public class UnrealLogParser
	{
		/// <summary>
		/// Our current content
		/// </summary>
		public string Content { get; protected set; }

		/// <summary>
		/// All entries in the log
		/// </summary>
		public IEnumerable<UnrealLog.LogEntry> LogEntries { get; protected set; }

		/// <summary>
		/// Summary of the log
		/// </summary>
		private UnrealLog Summary;

		// Track log levels we couldn't identify
		protected static HashSet<string> UnidentifiedLogLevels = new HashSet<string>();

		/// <summary>
		/// Constructor that takes the content to parse
		/// </summary>
		/// <param name="InContent"></param>
		/// <returns></returns>
		public UnrealLogParser(string InContent)
		{
			Content = SanitizeLogText(InContent);

			// Search for LogFoo: <Display|Error|etc>: Message
			// Also need to handle 'Log' not always being present, and the category being empty for a level of 'Log'
			MatchCollection MC = Regex.Matches(Content, @"(?<prefix>Log)?(?<category>[A-Za-z][\w\d]+):\s(?<level>Display|Verbose|VeryVerbose|Warning|Error|Fatal)?(?::\s)?(?<message>.*)");

			List<UnrealLog.LogEntry> ParsedEntries = new List<UnrealLog.LogEntry>();

			foreach (Match M in MC)
			{
				string Prefix = M.Groups["prefix"].ToString();
				string Category = M.Groups["category"].ToString();
				string LevelStr = M.Groups["level"].ToString();
				string Message = M.Groups["message"].ToString();

				UnrealLog.LogLevel Level = UnrealLog.LogLevel.Log;

				if (!string.IsNullOrEmpty(LevelStr))
				{
					if (!Enum.TryParse(LevelStr, out Level))
					{
						// only show a warning once
						if (!UnidentifiedLogLevels.Contains(LevelStr))
						{
							UnidentifiedLogLevels.Add(LevelStr);
							Log.Warning("Failed to match log level {0} to enum!", LevelStr);
						}
					}
				}

				ParsedEntries.Add(new UnrealLog.LogEntry(Prefix, Category, Level, Message));
			}

			LogEntries = ParsedEntries;
		}

		public static string SanitizeLogText(string InContent)
		{
			StringBuilder ContentBuilder = new StringBuilder();

			for (int BaseIdx = 0; BaseIdx < InContent.Length;)
			{
				// Extract the next line
				int EndIdx = InContent.IndexOf('\n', BaseIdx);
				if (EndIdx == -1)
				{
					break;
				}

				// Skip over any windows CR-LF line endings
				int LineEndIdx = EndIdx;
				if (LineEndIdx > BaseIdx && InContent[LineEndIdx - 1] == '\r')
				{
					LineEndIdx--;
				}

				// Render any JSON log events
				string Line = InContent.Substring(BaseIdx, LineEndIdx - BaseIdx);
				if (Line.Length > 0 && Line[0] == '{')
				{
					try
					{
						byte[] Buffer = Encoding.UTF8.GetBytes(Line);
						JsonLogEvent JsonEvent = JsonLogEvent.Parse(Buffer);
						Line = JsonEvent.GetLegacyLogLine();
					}
					catch (Exception ex)
					{
						EpicGames.Core.Log.Logger.LogDebug(ex, "Unable to parse log line: {Line}, Exception: {Ex}", Line, ex.ToString());

						int MinIdx = Math.Max(BaseIdx - 2048, 0);
						int MaxIdx = Math.Min(BaseIdx + 2048, InContent.Length);

						string[] Context = InContent.Substring(MinIdx, MaxIdx - MinIdx).Split('\n');
						for (int idx = 1; idx < Context.Length - 1; idx++)
						{
							EpicGames.Core.Log.Logger.LogDebug("Context {Idx}: {Line}", idx, Context[idx].TrimEnd());
						}
					}
				}

				ContentBuilder.Append(Line);
				ContentBuilder.Append('\n');

				// Move to the next line
				BaseIdx = EndIdx + 1;
			}

			return ContentBuilder.ToString();
		}

		public UnrealLog GetSummary()
		{
			if (Summary == null)
			{
				Summary = CreateSummary();
			}

			return Summary;
		}

		protected UnrealLog CreateSummary()
		{
			UnrealLog NewSummary = new UnrealLog();

			NewSummary.LoggedBuildInfo = GetBuildInfo();
			NewSummary.LoggedPlatformInfo = GetPlatformInfo();
			NewSummary.LogEntries = LogEntries;
			NewSummary.FatalError = GetFatalError();
			NewSummary.Ensures = GetEnsures();
			NewSummary.LineCount = Content.Split('\n').Count();
			NewSummary.HasTestExitCode = GetTestExitCode(out NewSummary.TestExitCode);

			NewSummary.EngineInitialized = GetAllMatches(@"LogInit.+Engine is initialized\.").Any();
			NewSummary.FullLogContent = this.Content;

			// Check request exit and reason
			RegexUtil.MatchAndApplyGroups(Content, @"Engine exit requested \(reason:\s*(.+)\)", (Groups) =>
			{
				NewSummary.RequestedExit = true;
				NewSummary.RequestedExitReason = Groups[1].ToString();
			});

			if (!NewSummary.RequestedExit)
			{
				string[] Completion = GetAllMatchingLines("F[a-zA-Z0-9]+::RequestExit");
				string[] ErrorCompletion = GetAllMatchingLines("StaticShutdownAfterError");

				if (Completion.Length > 0 || ErrorCompletion.Length > 0)
				{
					NewSummary.RequestedExit = true;
					NewSummary.RequestedExitReason = "Unidentified";
				}
			}

			return NewSummary;
		}


		/// <summary>
		/// Returns all lines from the specified content match the specified regex
		/// </summary>
		/// <param name="InContent"></param>
		/// <param name="InPattern"></param>
		/// <returns></returns>
		protected IEnumerable<Match> GetAllMatches(string InContent, string InPattern)
		{
			Regex regex = new Regex(InPattern);

			return regex.Matches(InContent).Cast<Match>();
		}

		/// <summary>
		/// Returns all lines from the specified content match the specified regex
		/// </summary>
		/// <param name="InContent"></param>
		/// <param name="InPattern"></param>
		/// <returns></returns>
		protected string[] GetAllMatchingLines(string InContent, string InPattern)
		{
			return GetAllMatches(InContent, InPattern).Select(M => M.Value).ToArray();
		}

		/// <summary>
		/// Returns all lines that match the specified regex
		/// </summary>
		/// <param name="InPattern"></param>
		/// <returns></returns>
		public string[] GetAllMatchingLines(string InPattern)
		{
			return GetAllMatchingLines(Content, InPattern);
		}

		/// <summary>
		/// Returns all Matches that match the specified regex
		/// </summary>
		/// <param name="InPattern"></param>
		/// <returns></returns>
		public IEnumerable<Match> GetAllMatches(string InPattern)
		{
			return GetAllMatches(Content, InPattern);
		}

		/// <summary>
		/// Returns a structure containing platform information extracted from the log
		/// </summary>
		/// <returns></returns>
		public UnrealLog.PlatformInfo GetPlatformInfo()
		{
			var Info = new UnrealLog.PlatformInfo();

			var InfoRegEx = @"LogInit.+OS:\s*(.+?)\s*(\((.+)\))?,\s*CPU:\s*(.+)\s*,\s*GPU:\s*(.+)";

			RegexUtil.MatchAndApplyGroups(Content, InfoRegEx, (Groups) =>
			{
				Info.OSName = Groups[1];
				Info.OSVersion = Groups[3];
				Info.CPUName = Groups[4];
				Info.GPUName = Groups[5];
			});

			return Info;
		}

		/// <summary>
		/// Returns a structure containing build information extracted from the log
		/// </summary>
		/// <returns></returns>
		public UnrealLog.BuildInfo GetBuildInfo()
		{
			var Info = new UnrealLog.BuildInfo();

			// pull from Branch Name: <name>
			Match M = Regex.Match(Content, @"LogInit.+Name:\s*(.*)", RegexOptions.IgnoreCase);

			if (M.Success)
			{
				Info.BranchName = M.Groups[1].ToString();
				Info.BranchName = Info.BranchName.Replace("+", "/");
			}

			M = Regex.Match(Content, @"LogInit.+CL-(\d+)", RegexOptions.IgnoreCase);

			if (M.Success)
			{
				Info.Changelist = Convert.ToInt32(M.Groups[1].ToString());
			}

			M = Regex.Match(Content, @"LogInit.+Build:\s*(\+.*)", RegexOptions.IgnoreCase);

			if (M.Success)
			{
				Info.BuildVersion = M.Groups[1].ToString();
			}

			return Info;
		}

		/// <summary>
		/// Returns all entries from the log that have the specified level
		/// </summary>
		/// <param name="InChannel">Optional channel to restrict search to</param>
		/// <returns></returns>
		public IEnumerable<UnrealLog.LogEntry> GetEntriesOfLevel(UnrealLog.LogLevel InLevel)
		{
			IEnumerable<UnrealLog.LogEntry> Entries = LogEntries.Where(E => E.Level == InLevel);
			return Entries;
		}

		/// <summary>
		/// Returns all warnings from the log
		/// </summary>
		/// <param name="InChannel">Optional channel to restrict search to</param>
		/// <returns></returns>
		public IEnumerable<UnrealLog.LogEntry> GetEntriesOfCategories(IEnumerable<string> InCategories, bool ExactMatch = false)
		{
			IEnumerable<UnrealLog.LogEntry> Entries;

			if (ExactMatch)
			{
				Entries = LogEntries.Where(E => InCategories.Contains(E.Category, StringComparer.OrdinalIgnoreCase));
			}
			else
			{
				// check if each channel is a substring of each log entry. E.g. "Shader" should return entries
				// with both ShaderCompiler and ShaderManager
				Entries = LogEntries.Where(E =>
				{
					string LogEntryCategory = E.Category.ToString();
					foreach (string Cat in InCategories)
					{
						if (LogEntryCategory.IndexOf(Cat, StringComparison.OrdinalIgnoreCase) >= 0)
						{
							return true;
						}
					}

					return false;
				});
			}
			return Entries;
		}

		/// <summary>
		/// Return all entries for the specified channel. E.g. "OrionGame" will
		/// return all entries starting with LogOrionGame
		/// </summary>
		/// <param name="Channel"></param>
		/// <returns></returns>
		public IEnumerable<string> GetLogChannels(IEnumerable<string> Channels, bool ExactMatch = true)
		{
			return GetEntriesOfCategories(Channels, ExactMatch).Select(E => E.ToString());
		}

		/// <summary>
		/// Returns channels that signify the editor doing stuff
		/// </summary>
		/// <returns></returns>
		public IEnumerable<string> GetEditorBusyChannels()
		{
			return GetLogChannels(UnrealLog.EditorBusyChannels, false);
		}

		/// <summary>
		/// Return all entries for the specified channel. E.g. "OrionGame" will
		/// return all entries starting with LogOrionGame
		/// </summary>
		/// <param name="Channel"></param>
		/// <returns></returns>
		public IEnumerable<string> GetLogChannel(string Channel, bool ExactMatch = true)
		{
			return GetLogChannels(new string[] { Channel }, ExactMatch);
		}


		/// <summary>
		/// Returns all warnings from the log
		/// </summary>
		/// <param name="InChannel">Optional channel to restrict search to</param>
		/// <returns></returns>
		public IEnumerable<string> GetWarnings(string InChannel = null)
		{
			IEnumerable<UnrealLog.LogEntry> Entries = LogEntries.Where(E => E.Level == UnrealLog.LogLevel.Warning);

			if (InChannel != null)
			{
				Entries = Entries.Where(E => E.Category.Equals(InChannel, StringComparison.OrdinalIgnoreCase));
			}

			return Entries.Select(E => E.ToString());
		}

		/// <summary>
		/// Returns all errors from the log
		/// </summary>
		/// <param name="InChannel">Optional channel to restrict search to</param>
		/// <returns></returns>
		public IEnumerable<string> GetErrors(string InChannel = null)
		{
			IEnumerable<UnrealLog.LogEntry> Entries = LogEntries.Where(E => E.Level == UnrealLog.LogLevel.Error);

			if (InChannel != null)
			{
				Entries = Entries.Where(E => E.Category.Equals(InChannel, StringComparison.OrdinalIgnoreCase));
			}

			return Entries.Select(E => E.ToString());
		}

		/// <summary>
		/// Returns all ensures from the log
		/// </summary>
		/// <returns></returns>
		public IEnumerable<UnrealLog.CallstackMessage> GetEnsures()
		{
			IEnumerable<UnrealLog.CallstackMessage> Ensures = ParseTracedErrors(new[] { @"Log.+:\s{0,1}Error:\s{0,1}(Ensure condition failed:.+)" }, false);

			foreach (UnrealLog.CallstackMessage Error in Ensures)
			{
				Error.IsEnsure = true;
			}

			return Ensures;
		}

		/// <summary>
		/// If the log contains a fatal error return that information
		/// </summary>
		/// <param name="ErrorInfo"></param>
		/// <returns></returns>
		public UnrealLog.CallstackMessage GetFatalError()
		{
			string[] ErrorMsgMatches = new string[] { @"(Fatal Error:.+)", @"Critical error: =+[\s\n]+(?:.+?\s*Error:\s*)?(.+)", @"(Assertion Failed:.+)", @"(Unhandled Exception:.+)", @"(LowLevelFatalError.+)" };

			var Traces = ParseTracedErrors(ErrorMsgMatches).Concat(GetASanErrors());

			// Keep the one with the most information.
			Traces.OrderBy(T => T.Callstack.Length);
			return Traces.Count() > 0 ? Traces.Last() : null;
		}

		/// <summary>
		/// Parse the log for Address Sanitizer error.
		/// </summary>
		/// <returns></returns>
		public IEnumerable<UnrealLog.CallstackMessage> GetASanErrors()
		{
			/// Match:
			/// ==5077==ERROR: AddressSanitizer: alloc - dealloc - mismatch(operator new vs free) on 0x602014ab4790
			/// Then for gathering the callstack, match
			/// ==5077==ABORTING
			/// remove anything inside the callstack starting with [2022.12.02-15.22.40:688][618]

			List<UnrealLog.CallstackMessage> ASanReports = new List<UnrealLog.CallstackMessage>();
			string InitPattern = @"==([0-9]+)==ERROR: ([^\n]+)";
			string LineStartsWithTimeStamp = @"^\[[0-9.:-]+\]\[[0-9]+\]";

			MatchCollection Matches = Regex.Matches(Content, InitPattern, RegexOptions.IgnoreCase);

			foreach (Match TraceInitMatch in Matches)
			{
				string TraceID = TraceInitMatch.Groups[1].Value;
				string EndPattern = @$"=={TraceID}==ABORTING";
				int TraceInitIndex = TraceInitMatch.Index + TraceInitMatch.Length;

				UnrealLog.CallstackMessage NewTrace = new UnrealLog.CallstackMessage();
				NewTrace.Position = TraceInitMatch.Index;
				NewTrace.Message = TraceInitMatch.Groups[2].Value;

				// If the regex matches the very end of the string, the substring will get an invalid range.
				string ErrorContent = Content.Length <= TraceInitIndex
					? string.Empty : Content.Substring(TraceInitIndex + 1);

				if (!string.IsNullOrEmpty(ErrorContent))
				{
					Match MsgMatch = Regex.Match(ErrorContent, EndPattern);
					if (MsgMatch.Success)
					{
						string MsgContent = ErrorContent.Substring(0, MsgMatch.Index);
						// Prune the line with time stamp
						List<string> Backtrace = new List<string>();
						foreach (string Line in MsgContent.Split("\n"))
						{
							Match IsTimeStampLine = Regex.Match(Line, LineStartsWithTimeStamp);
							if (!IsTimeStampLine.Success)
							{
								Backtrace.Add(Line);
							}
						}

						NewTrace.Callstack = Backtrace.ToArray();
					}
				}

				if(NewTrace.Callstack == null)
				{
					NewTrace.Callstack = new string[] { "Unable to parse callstack from log" };
				}

				ASanReports.Add(NewTrace);
			}

			return ASanReports;
		}

		/// <summary>
		/// Returns true if the log contains a test complete marker
		/// </summary>
		/// <returns></returns>
		public bool HasTestCompleteMarker()
		{
			string[] Completion = GetAllMatchingLines(@"\*\*\* TEST COMPLETE.+");

			return Completion.Length > 0;
		}

		/// <summary>
		/// Returns true if the log contains a request to exit that was not due to an error
		/// </summary>
		/// <returns></returns>
		public bool HasRequestExit()
		{
			return GetSummary().RequestedExit;
		}

		/// <summary>
		/// Returns a block of lines that start and end with the specified regex patterns
		/// </summary>
		/// <param name="StartPattern">Regex to match the first line</param>
		/// <param name="EndPattern">Regex to match the final line</param>
		/// <param name="PatternOptions">Optional RegexOptions applied to both patterns. IgnoreCase by default.</param>
		/// <returns>Array of strings for each found block of lines. Lines within each string are delimited by newline character.</returns>
		public string[] GetGroupsOfLinesBetween(string StartPattern, string EndPattern, RegexOptions PatternOptions = RegexOptions.IgnoreCase)
		{
			Regex StartRegex = new Regex(StartPattern, PatternOptions);
			Regex EndRegex = new Regex(EndPattern, PatternOptions);
			List<string> Blocks = new List<string>();

			foreach (Match StartMatch in StartRegex.Matches(Content))
			{
				int StartIndex = Content.LastIndexOf('\n', StartMatch.Index) + 1;
				Match EndMatch = EndRegex.Match(Content, StartMatch.Index + StartMatch.Length);
				int EndIndex = Content.IndexOf('\n', EndMatch.Index);

				if (EndIndex > StartIndex)
				{
					string Block = Content.Substring(StartIndex, EndIndex - StartIndex);

					Blocks.Add(Block);
				}
			}

			return Blocks.ToArray();
		}

		/// <summary>
		/// Returns a block of lines that start with the specified regex
		/// </summary>
		/// <param name="Pattern">Regex to match the first line</param>
		/// <param name="LineCount">Number of lines in the returned block</param>
		/// <returns>Array of strings for each found block of lines. Lines within each string are delimited by newline character.</returns>
		public string[] GetGroupsOfLinesStartingWith(string Pattern, int LineCount, RegexOptions PatternOptions = RegexOptions.IgnoreCase)
		{
			Regex regex = new Regex(Pattern, PatternOptions);

			List<string> Blocks = new List<string>();

			foreach (Match match in regex.Matches(Content))
			{
				int Location = match.Index;

				int Start = Content.LastIndexOf('\n', Location) + 1;
				int End = Location;

				for (int i = 0; i < LineCount; i++)
				{
					End = Content.IndexOf('\n', End) + 1;
				}

				if (End > Start)
				{
					string Block = Content.Substring(Start, End - Start);

					Blocks.Add(Block);
				}
			}

			return Blocks.ToArray();
		}

		/// <summary>
		/// Finds all callstack-based errors with the specified pattern
		/// </summary>
		/// <param name="Patterns"></param>
		/// <returns></returns>
		protected IEnumerable<UnrealLog.CallstackMessage> ParseTracedErrors(string[] Patterns, bool IncludePostmortem = true)
		{
			List<UnrealLog.CallstackMessage> Traces = new List<UnrealLog.CallstackMessage>();

			// As well as what was requested, search for a postmortem stack...
			IEnumerable<string> AllPatterns = IncludePostmortem ? Patterns.Concat(new[] { "(Postmortem Cause:.*)" }) : Patterns;

			// Try and find an error message
			foreach (string Pattern in AllPatterns)
			{
				MatchCollection Matches = Regex.Matches(Content, Pattern, RegexOptions.IgnoreCase);

				foreach (Match TraceMatch in Matches)
				{
					UnrealLog.CallstackMessage NewTrace = new UnrealLog.CallstackMessage();

					NewTrace.Position = TraceMatch.Index;
					NewTrace.Message = TraceMatch.Groups[1].Value;

					// If the regex matches the very end of the string, the substring will get an invalid range.
					string ErrorContent = Content.Length <= TraceMatch.Index + TraceMatch.Length
						? string.Empty : Content.Substring(TraceMatch.Index + TraceMatch.Length + 1);

					Match MsgMatch = Regex.Match(ErrorContent, @".+:\s*Error:\s*(.+)");

					if (MsgMatch.Success)
					{
						string MsgString = MsgMatch.Groups[1].ToString();
						if (!MsgMatch.Groups[0].ToString().Contains("\n") /* avoid a bug where .+ match \n */
							&& !string.IsNullOrEmpty(MsgString)
							&& Regex.Match(MsgString, @"0[xX][0-9A-f]{8,16}").Success == false)
						{
							NewTrace.Message = NewTrace.Message + "\n" + MsgString;
						}
					}

					//
					// Handing callstacks-
					//
					// Unreal now uses a canonical format for printing callstacks during errors which is 
					//
					//0xaddress module!func [file]
					// 
					// E.g. 0x045C8D01 OrionClient.self!UEngine::PerformError() [D:\Epic\Orion\Engine\Source\Runtime\Engine\Private\UnrealEngine.cpp:6481]
					//
					// Module may be omitted, everything else should be present, or substituted with a string that conforms to the expected type
					//
					// E.g 0x00000000 UnknownFunction []
					//
					// A calstack as part of an ensure, check, or exception will look something like this -
					// 
					//
					//[2017.08.21-03.28.40:667][313]LogWindows:Error: Assertion failed: false [File:D:\Epic\Orion\Release-Next\Engine\Plugins\NotForLicensees\Gauntlet\Source\Gauntlet\Private\GauntletTestControllerErrorTest.cpp] [Line: 29] 
					//[2017.08.21-03.28.40:667][313]LogWindows:Error: Asserting as requested
					//[2017.08.21-03.28.40:667][313]LogWindows:Error: 
					//[2017.08.21-03.28.40:667][313]LogWindows:Error: 
					//[2017.08.21-03.28.40:667][313]LogWindows:Error: [Callstack] 0x00000000FDC2A06D KERNELBASE.dll!UnknownFunction []
					//[2017.08.21-03.28.40:667][313]LogWindows:Error: [Callstack] 0x00000000418C0119 OrionClient.exe!FOutputDeviceWindowsError::Serialize() [d:\epic\orion\release-next\engine\source\runtime\core\private\windows\windowsplatformoutputdevices.cpp:120]
					//[2017.08.21-03.28.40:667][313]LogWindows:Error: [Callstack] 0x00000000416AC12B OrionClient.exe!FOutputDevice::Logf__VA() [d:\epic\orion\release-next\engine\source\runtime\core\private\misc\outputdevice.cpp:70]
					//[2017.08.21-03.28.40:667][313]LogWindows:Error: [Callstack] 0x00000000418BD124 OrionClient.exe!FDebug::AssertFailed() [d:\epic\orion\release-next\engine\source\runtime\core\private\misc\assertionmacros.cpp:373]
					//[2017.08.21-03.28.40:668][313]LogWindows:Error: [Callstack] 0x000000004604A879 OrionClient.exe!UGauntletTestControllerErrorTest::OnTick() [d:\epic\orion\release-next\engine\plugins\notforlicensees\gauntlet\source\gauntlet\private\gauntlettestcontrollererrortest.cpp:29]
					//[2017.08.21-03.28.40:668][313]LogWindows:Error: [Callstack] 0x0000000046049166 OrionClient.exe!FGauntletModuleImpl::InnerTick() [d:\epic\orion\release-next\engine\plugins\notforlicensees\gauntlet\source\gauntlet\private\gauntletmodule.cpp:315]
					//[2017.08.21-03.28.40:668][313]LogWindows:Error: [Callstack] 0x0000000046048472 OrionClient.exe!TBaseFunctorDelegateInstance<bool __cdecl(float),<lambda_b2e6da8e95d7ed933c391f0ec034aa11> >::Execute() [d:\epic\orion\release-next\engine\source\runtime\core\public\delegates\delegateinstancesimpl.h:1132]
					//[2017.08.21-03.28.40:668][313]LogWindows:Error: [Callstack] 0x00000000415101BE OrionClient.exe!FTicker::Tick() [d:\epic\orion\release-next\engine\source\runtime\core\private\containers\ticker.cpp:82]
					//[2017.08.21-03.28.40:668][313]LogWindows:Error: [Callstack] 0x00000000402887DD OrionClient.exe!FEngineLoop::Tick() [d:\epic\orion\release-next\engine\source\runtime\launch\private\launchengineloop.cpp:3295]
					//[2017.08.21-03.28.40:668][313]LogWindows:Error: [Callstack] 0x00000000402961FC OrionClient.exe!GuardedMain() [d:\epic\orion\release-next\engine\source\runtime\launch\private\launch.cpp:166]
					//[2017.08.21-03.28.40:668][313]LogWindows:Error: [Callstack] 0x000000004029625A OrionClient.exe!GuardedMainWrapper() [d:\epic\orion\release-next\engine\source\runtime\launch\private\windows\launchwindows.cpp:134]
					//[2017.08.21-03.28.40:668][313]LogWindows:Error: [Callstack] 0x00000000402A2D68 OrionClient.exe!WinMain() [d:\epic\orion\release-next\engine\source\runtime\launch\private\windows\launchwindows.cpp:210]
					//[2017.08.21-03.28.40:669][313]LogWindows:Error: [Callstack] 0x0000000046EEC0CB OrionClient.exe!__scrt_common_main_seh() [f:\dd\vctools\crt\vcstartup\src\startup\exe_common.inl:253]
					//[2017.08.21-03.28.40:669][313]LogWindows:Error: [Callstack] 0x0000000077A759CD kernel32.dll!UnknownFunction []
					//[2017.08.21-03.28.40:669][313]LogWindows:Error: [Callstack] 0x0000000077CAA561 ntdll.dll!UnknownFunction []
					//[2017.08.21-03.28.40:669][313]LogWindows:Error: [Callstack] 0x0000000077CAA561 ntdll.dll!UnknownFunction []
					//
					// So the code below starts at the point of the error message, and searches subsequent lines for things that look like a callstack. If we go too many lines without 
					// finding one then we break. Note that it's possible that log messages from another thread may be intermixed, so we can't just break on a change of verbosity or 
					// channel
					// 

					string SearchContent = ErrorContent;

					int LinesWithoutBacktrace = 0;

					List<string> Backtrace = new List<string>();

					do
					{
						int EOL = SearchContent.IndexOf("\n");

						if (EOL == -1)
						{
							break;
						}

						string Line = SearchContent.Substring(0, EOL);

						// collapse inline function
						Line = Line.Replace("[Inline Function] ", "[InlineFunction]");

						// Must have [Callstack] 0x00123456
						// The module name is optional, must start with whitespace, and continues until next whites[ace
						// filename is optional, must be in [file]
						// module address is optional, must be in quotes() after address
						// Note - Unreal callstacks are always meant to omit all three with placeholders for missing values, but
						// we'll assume that may not happen...
						Match CSMatch = Regex.Match(Line, @"(0[xX][0-9A-f]{8,16})(?:\s+\(0[xX][0-9A-f]{8,16}\))?\s+(.+)\s+\[(.*?)\]$", RegexOptions.IgnoreCase);

						if (CSMatch.Success)
						{
							string Address = CSMatch.Groups[1].Value;
							string Func = CSMatch.Groups[2].Value;
							string File = CSMatch.Groups[3].Value;

							if (string.IsNullOrEmpty(File))
							{
								File = "Unknown File";
							}

							// Remove any exe
							const string StripFrom = ".exe!";

							if (Func.IndexOf(StripFrom) > 0)
							{
								Func = Func.Substring(Func.IndexOf(StripFrom) + StripFrom.Length);
							}

							string NewLine = string.Format("{0} {1} [{2}]", Address, Func, File);

							Backtrace.Add(NewLine);

							LinesWithoutBacktrace = 0;
						}
						else
						{
							LinesWithoutBacktrace++;
						}

						SearchContent = SearchContent.Substring(EOL + 1);

					} while (LinesWithoutBacktrace < 10);

					if (Backtrace.Count > 0)
					{
						NewTrace.Callstack = Backtrace.Distinct().ToArray();
					}
					else
					{
						NewTrace.Callstack = new[] { "Unable to parse callstack from log" };
					}
					Traces.Add(NewTrace);
				}
			}

			// Now, because platforms sometimes dump asserts to the log and low-level logging, and we might have a post-mortem stack, we
			// need to prune out redundancies. Basic approach - find errors with the same assert message and keep the one with the
			// longest callstack. If we have a post-mortem error, overwrite the previous trace with its info (on some devices the post-mortem
			// info is way more informative).

			List<UnrealLog.CallstackMessage> FilteredTraces = new List<UnrealLog.CallstackMessage>();


			for (int i = 0; i < Traces.Count; i++)
			{
				var Trace = Traces[i];

				// check the next trace to see if it's a dupe of us
				if (i + 1 < Traces.Count)
				{
					var NextTrace = Traces[i + 1];

					if (Trace.Message.Equals(NextTrace.Message, StringComparison.OrdinalIgnoreCase))
					{
						if (Trace.Callstack.Length < NextTrace.Callstack.Length)
						{
							Trace.Callstack = NextTrace.Callstack;
							// skip the next error as we stole its callstack already
							i++;
						}
					}
				}

				// check this trace to see if it's postmortem
				if (Trace.Message.IndexOf("Postmortem Cause:", StringComparison.OrdinalIgnoreCase) != -1)
				{
					// we have post-mortem info, which should be much better than the game-generated stuff and will be sorted to first position
					Trace.IsPostMortem = true;
				}

				FilteredTraces.Add(Trace);
			}

			// If we have a post mortem crash, sort it to the front
			if (FilteredTraces.FirstOrDefault((Trace) => { return Trace.IsPostMortem; }) != null)
			{
				FilteredTraces.Sort((Trace1, Trace2) => { if (!Trace1.IsPostMortem && !Trace2.IsPostMortem) return 0; return Trace1.IsPostMortem ? -1 : 1; });
			}

			return FilteredTraces;
		}

		/// <summary>
		/// Attempts to find an exit code for a test
		/// </summary>
		/// <param name="ExitCode"></param>
		/// <returns></returns>
		public bool GetTestExitCode(out int ExitCode)
		{
			Regex Reg = new Regex(@"\*\s+TEST COMPLETE. EXIT CODE:\s*(-?\d?)\s+\*");

			Match M = Reg.Match(Content);

			if (M.Groups.Count > 1)
			{
				ExitCode = Convert.ToInt32(M.Groups[1].Value);
				return true;
			}

			Reg = new Regex(@"RequestExitWithStatus\(\d+,\s*(\d+).*\)");

			M = Reg.Match(Content);

			if (M.Groups.Count > 1)
			{
				ExitCode = Convert.ToInt32(M.Groups[1].Value);
				return true;
			}

			if (Content.Contains("EnvironmentalPerfTest summary"))
			{
				Log.Warning("Found - 'EnvironmentalPerfTest summary', using temp workaround and assuming success (!)");
				ExitCode = 0;
				return true;
			}

			ExitCode = -1;
			return false;
		}
	}
}