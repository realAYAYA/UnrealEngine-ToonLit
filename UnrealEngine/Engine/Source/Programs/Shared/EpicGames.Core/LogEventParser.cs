// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using Microsoft.Extensions.Logging;

namespace EpicGames.Core
{
	/// <summary>
	/// Confidence of a matched log event being the correct derivation
	/// </summary>
	public enum LogEventPriority
	{
		/// <summary>
		/// Unspecified priority
		/// </summary>
		None,

		/// <summary>
		/// Lowest confidence match
		/// </summary>
		Lowest,

		/// <summary>
		/// Low confidence match
		/// </summary>
		Low,

		/// <summary>
		/// Below normal confidence match
		/// </summary>
		BelowNormal,

		/// <summary>
		/// Normal confidence match
		/// </summary>
		Normal,

		/// <summary>
		/// Above normal confidence match
		/// </summary>
		AboveNormal,

		/// <summary>
		/// High confidence match
		/// </summary>
		High,

		/// <summary>
		/// Highest confidence match
		/// </summary>
		Highest,
	}

	/// <summary>
	/// Information about a matched event
	/// </summary>
	public class LogEventMatch
	{
		/// <summary>
		/// Confidence of the match
		/// </summary>
		public LogEventPriority Priority { get; }

		/// <summary>
		/// Matched events
		/// </summary>
		public List<LogEvent> Events { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public LogEventMatch(LogEventPriority priority, LogEvent logEvent)
		{
			Priority = priority;
			Events = new List<LogEvent> { logEvent };
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public LogEventMatch(LogEventPriority priority, IEnumerable<LogEvent> events)
		{
			Priority = priority;
			Events = events.ToList();
		}
	}

	/// <summary>
	/// Interface for a class which matches error strings
	/// </summary>
	public interface ILogEventMatcher
	{
		/// <summary>
		/// Attempt to match events from the given input buffer
		/// </summary>
		/// <param name="cursor">The input buffer</param>
		/// <returns>Information about the error that was matched, or null if an error was not matched</returns>
		LogEventMatch? Match(ILogCursor cursor);
	}

	/// <summary>
	/// Turns raw text output into structured logging events
	/// </summary>
	public class LogEventParser : IDisposable
	{
		/// <summary>
		/// List of event matchers for this parser
		/// </summary>
		public List<ILogEventMatcher> Matchers { get; } = new List<ILogEventMatcher>();

		/// <summary>
		/// List of patterns to ignore
		/// </summary>
		public List<Regex> IgnorePatterns { get; } = new List<Regex>();

		/// <summary>
		/// Buffer of input lines
		/// </summary>
		readonly LogBuffer _buffer;

		/// <summary>
		/// Buffer for holding partial line data
		/// </summary>
		readonly ByteArrayBuilder _partialLine = new ByteArrayBuilder();

		/// <summary>
		/// Whether matching is currently enabled
		/// </summary>
		int _matchingEnabled;

		/// <summary>
		/// The inner logger
		/// </summary>
		ILogger _logger;

		/// <summary>
		/// Log events sinks in addition to <see cref="_logger" />
		/// </summary>
		readonly List<ILogEventSink> _logEventSinks = new List<ILogEventSink>();

		/// <summary>
		/// Timer for the parser being active
		/// </summary>
		readonly Stopwatch _timer = Stopwatch.StartNew();

		/// <summary>
		/// Amount of time that the log parser has been processing events
		/// </summary>
		readonly Stopwatch _activeTimer = new Stopwatch();

		/// <summary>
		/// Number of lines parsed in the last interval
		/// </summary>
		int _linesParsed = 0;

		/// <summary>
		/// Public accessor for the logger
		/// </summary>
		public ILogger Logger
		{
			get => _logger;
			set => _logger = value;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="logger">The logger to receive parsed output messages</param>
		/// <param name="logEventSinks">Additional sinks to receive log events</param>
		public LogEventParser(ILogger logger, List<ILogEventSink>? logEventSinks = null)
		{
			_logger = logger;
			_buffer = new LogBuffer(50);
			
			if (logEventSinks != null)
			{
				_logEventSinks.AddRange(logEventSinks);
			}
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			Dispose(true);
			GC.SuppressFinalize(this);
		}

		/// <summary>
		/// Standard Dispose pattern method
		/// </summary>
		/// <param name="disposing"></param>
		protected virtual void Dispose(bool disposing)
		{
			if (disposing)
			{
				Flush();
			}
		}

		/// <summary>
		/// Enumerate all the types that implement <see cref="ILogEventMatcher"/> in the given assembly, and create instances of them
		/// </summary>
		/// <param name="assembly">The assembly to enumerate matchers from</param>
		public void AddMatchersFromAssembly(Assembly assembly)
		{
			foreach (Type type in assembly.GetTypes())
			{
				if (type.IsClass && typeof(ILogEventMatcher).IsAssignableFrom(type))
				{
					_logger.LogDebug("Adding event matcher: {Type}", type.Name);
					try
					{
						ILogEventMatcher matcher = (ILogEventMatcher)Activator.CreateInstance(type)!;
						Matchers.Add(matcher);
					}
					catch (Exception ex)
					{
						_logger.LogDebug("Failed to add event matcher for {Type}: {Exception}", type.Name, ex.ToString());
					}
				}
			}
		}

		/// <summary>
		/// Read ignore patterns from the given root directory
		/// </summary>
		/// <param name="rootDir"></param>
		/// <returns></returns>
		public async Task ReadIgnorePatternsAsync(DirectoryReference rootDir)
		{
			Stopwatch timer = Stopwatch.StartNew();

			List<DirectoryReference> baseDirs = new List<DirectoryReference>();
			baseDirs.Add(rootDir);
			AddRestrictedDirs(baseDirs, "Restricted");
			AddRestrictedDirs(baseDirs, "Platforms");

			List<(FileReference, Task)> tasks = new List<(FileReference, Task)>();
			foreach (DirectoryReference baseDir in baseDirs)
			{
				FileReference ignorePatternFile = FileReference.Combine(baseDir, "Build", "Horde", "IgnorePatterns.txt");
				if (FileReference.Exists(ignorePatternFile))
				{
					_logger.LogDebug("Reading ignore patterns from {File}...", ignorePatternFile);
					tasks.Add((ignorePatternFile, ReadIgnorePatternsAsync(ignorePatternFile)));
				}
			}

			foreach ((FileReference file, Task task) in tasks)
			{
				try
				{
					await task;
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Exception reading patterns from {File}: {Message}", file, ex.Message);
				}
			}

			_logger.LogDebug("Took {TimeMs}ms to read ignore patterns", timer.ElapsedMilliseconds);
		}

		/// <summary>
		/// Read ignore patterns from a single file
		/// </summary>
		/// <param name="ignorePatternFile"></param>
		/// <returns></returns>
		public async Task ReadIgnorePatternsAsync(FileReference ignorePatternFile)
		{
			string[] lines = await FileReference.ReadAllLinesAsync(ignorePatternFile);

			List<Regex> patterns = new List<Regex>();
			foreach (string line in lines)
			{
				string trimLine = line.Trim();
				if (trimLine.Length > 0 && trimLine[0] != '#')
				{
					patterns.Add(new Regex(trimLine));
				}
			}

			lock (IgnorePatterns)
			{
				IgnorePatterns.AddRange(patterns);
			}
		}

		static void AddRestrictedDirs(List<DirectoryReference> directories, string subFolder)
		{
			int numDirs = directories.Count;
			for (int idx = 0; idx < numDirs; idx++)
			{
				DirectoryReference subDir = DirectoryReference.Combine(directories[idx], subFolder);
				if (DirectoryReference.Exists(subDir))
				{
					directories.AddRange(DirectoryReference.EnumerateDirectories(subDir));
				}
			}
		}

		/// <summary>
		/// Writes a line to the event filter
		/// </summary>
		/// <param name="line">The line to output</param>
		public void WriteLine(string line)
		{
			if (line.Length > 0 && line[0] == '{')
			{
				int length = line.Length;
				while(length > 0 && Char.IsWhiteSpace(line[length - 1]))
				{
					length--;
				}

				byte[] data = Encoding.UTF8.GetBytes(line, 0, length);
				try
				{
					JsonLogEvent jsonEvent;
					if (JsonLogEvent.TryParse(data, out jsonEvent))
					{
						ProcessData(true);
						_logger.Log(jsonEvent.Level, jsonEvent.EventId, jsonEvent, null, JsonLogEvent.Format);
						return;
					}
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Exception while parsing log event: {Message}", ex.Message);
				}
			}

			_buffer.AddLine(StringUtils.ParseEscapeCodes(line));
			ProcessData(false);
		}

		/// <summary>
		/// Writes data to the log parser
		/// </summary>
		/// <param name="data">Data to write</param>
		public void WriteData(ReadOnlyMemory<byte> data)
		{
			int baseIdx = 0;
			int scanIdx = 0;
			ReadOnlySpan<byte> span = data.Span;

			// Handle a partially existing line
			if (_partialLine.Length > 0)
			{
				for (; scanIdx < span.Length; scanIdx++)
				{
					if (span[scanIdx] == '\n')
					{
						_partialLine.WriteFixedLengthBytes(span.Slice(baseIdx, scanIdx - baseIdx));
						FlushPartialLine();
						baseIdx = ++scanIdx;
						break;
					}
				}
			}

			// Handle any complete lines
			for (; scanIdx < span.Length; scanIdx++)
			{
				if(span[scanIdx] == '\n')
				{
					AddLine(data.Slice(baseIdx, scanIdx - baseIdx));
					baseIdx = scanIdx + 1;
				}
			}

			// Add the rest of the text to the partial line buffer
			_partialLine.WriteFixedLengthBytes(span.Slice(baseIdx));

			// Process the new data
			ProcessData(false);
		}

		/// <summary>
		/// Flushes the current contents of the parser
		/// </summary>
		public void Flush()
		{
			// If there's a partially written line, write that out first
			if (_partialLine.Length > 0)
			{
				FlushPartialLine();
			}

			// Process any remaining data
			ProcessData(true);
		}

		/// <summary>
		/// Adds a raw utf-8 string to the buffer
		/// </summary>
		/// <param name="data">The string data</param>
		private void AddLine(ReadOnlyMemory<byte> data)
		{
			if (data.Length > 0 && data.Span[data.Length - 1] == '\r')
			{
				data = data.Slice(0, data.Length - 1);
			}
			if (data.Length > 0 && data.Span[0] == '{')
			{
				JsonLogEvent jsonEvent;
				if (JsonLogEvent.TryParse(data, out jsonEvent))
				{
					ProcessData(true);
					_logger.LogJsonLogEvent(jsonEvent);
					return;
				}
			}
			_buffer.AddLine(StringUtils.ParseEscapeCodes(Encoding.UTF8.GetString(data.Span)));
		}

		/// <summary>
		/// Writes the current partial line data, with the given data appended to it, then clear the buffer
		/// </summary>
		private void FlushPartialLine()
		{
			AddLine(_partialLine.ToByteArray());
			_partialLine.Clear();
		}

		/// <summary>
		/// Process any data in the buffer
		/// </summary>
		/// <param name="bFlush">Whether we've reached the end of the stream</param>
		void ProcessData(bool bFlush)
		{
			_activeTimer.Start();
			int startLineCount = _buffer.Length;
			while (_buffer.Length > 0)
			{
				// Try to match an event
				List<LogEvent>? events = null;
				if (Regex.IsMatch(_buffer[0]!, "<-- Suspend Log Parsing -->", RegexOptions.IgnoreCase))
				{
					_matchingEnabled--;
				}
				else if (Regex.IsMatch(_buffer[0]!, "<-- Resume Log Parsing -->", RegexOptions.IgnoreCase))
				{
					_matchingEnabled++;
				}
				else if (_matchingEnabled >= 0)
				{
					events = MatchEvent();
				}

				// Bail out if we need more data
				if (_buffer.Length < 1024 && !bFlush && _buffer.NeedMoreData)
				{
					break;
				}

				// If we did match something, check if it's not negated by an ignore pattern. We typically have relatively few errors and many more ignore patterns than matchers, so it's quicker 
				// to check them in response to an identified error than to treat them as matchers of their own.
				if (events != null)
				{
					foreach (Regex ignorePattern in IgnorePatterns)
					{
						if (ignorePattern.IsMatch(_buffer[0]!))
						{
							events = null;
							break;
						}
					}
				}

				// Report the error to the listeners
				if (events != null)
				{
					WriteEvents(events);
					_buffer.Advance(events.Count);
				}
				else
				{
					_logger.Log(LogLevel.Information, KnownLogEvents.None, _buffer[0]!, null, (state, exception) => state);
					_buffer.MoveNext();
				}
			}
			_linesParsed += startLineCount - _buffer.Length;
			_activeTimer.Stop();

			const double UpdateIntervalSeconds = 30.0;
			double elapsedSeconds = _timer.Elapsed.TotalSeconds;
			if (elapsedSeconds > UpdateIntervalSeconds)
			{
				const double WarnPct = 0.5;
				double activeSeconds = _activeTimer.Elapsed.TotalSeconds;
				double activePct = activeSeconds / elapsedSeconds;

				if (activePct > WarnPct)
				{
					_logger.LogInformation(KnownLogEvents.Systemic_LogParserBottleneck, "EpicGames.Core.LogEventParser is taking a significant amount of CPU time: {Active:n1}s/{Total:n1}s ({Pct:n1}%). Processed {NumLines} lines in last {Interval} seconds ({NumLinesInBuffer} in buffer).", activeSeconds, elapsedSeconds, activePct * 100.0, _linesParsed, UpdateIntervalSeconds, _buffer.Length);
				}

				_activeTimer.Reset();
				_timer.Restart();
				_linesParsed = 0;
			}
		}

		/// <summary>
		/// Try to match an event from the current buffer
		/// </summary>
		/// <returns>The matched event</returns>
		private List<LogEvent>? MatchEvent()
		{
			LogEventMatch? currentMatch = null;
			foreach (ILogEventMatcher matcher in Matchers)
			{
				LogEventMatch? match = null;
				try
				{
					match = matcher.Match(_buffer);
				}
				catch (Exception ex)
				{
					_logger.LogWarning(KnownLogEvents.Systemic_LogEventMatcher, ex, "Exception while parsing log events with {Type}. Buffer size {Length}.", matcher.GetType().Name, _buffer.Length);
				}
				if(match != null)
				{
					if (currentMatch == null || match.Priority > currentMatch.Priority)
					{
						currentMatch = match;
					}
				}
			}
			return currentMatch?.Events;
		}

		/// <summary>
		/// Writes an event to the log
		/// </summary>
		/// <param name="logEvents">The event to write</param>
		protected virtual void WriteEvents(List<LogEvent> logEvents)
		{
			foreach (LogEvent logEvent in logEvents)
			{
				_logger.Log(logEvent.Level, logEvent.Id, logEvent, null, (state, exception) => state.ToString());
				foreach (ILogEventSink logEventSink in _logEventSinks)
				{
					logEventSink.ProcessEvent(logEvent);
				}
			}
		}
	}
}
