// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics.CodeAnalysis;
using System.Text;
using System.Text.Json;
using Microsoft.Extensions.Logging;

namespace EpicGames.Core
{
	/// <summary>
	/// Defines a preformatted Json log event, which can pass through raw Json data directly or format it as a regular string
	/// </summary>
	public struct JsonLogEvent
	{
		/// <summary>
		/// The log level
		/// </summary>
		public LogLevel Level { get; }

		/// <summary>
		/// The event id, if set
		/// </summary>
		public EventId EventId { get; }

		/// <summary>
		/// Index of this line
		/// </summary>
		public int LineIndex { get; }

		/// <summary>
		/// Number of lines in a multi-line message
		/// </summary>
		public int LineCount { get; }

		/// <summary>
		/// The utf-8 encoded JSON event
		/// </summary>
		public ReadOnlyMemory<byte> Data { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public JsonLogEvent(LogLevel level, EventId eventId, int lineIndex, int lineCount, ReadOnlyMemory<byte> data)
		{
			Level = level;
			EventId = eventId;
			LineIndex = lineIndex;
			LineCount = lineCount;
			Data = data;
		}

		/// <summary>
		/// Creates a json log event from the given logger paramters
		/// </summary>
		/// <inheritdoc cref="ILogger.Log{TState}(LogLevel, EventId, TState, Exception, Func{TState, Exception, String})"/>
		public static JsonLogEvent FromLoggerState<TState>(LogLevel logLevel, EventId eventId, TState state, Exception? exception, Func<TState, Exception?, string> formatter)
		{
			if (state is JsonLogEvent jsonLogEvent)
			{
				return jsonLogEvent;
			}

			LogEvent? logEvent = state as LogEvent;
			if (logEvent == null)
			{
				logEvent = LogEvent.FromState(logLevel, eventId, state, exception, formatter);
			}

			return new JsonLogEvent(logLevel, eventId, 0, 1, logEvent.ToJsonBytes());
		}

		/// <summary>
		/// Parse an event from the given data
		/// </summary>
		/// <param name="data"></param>
		/// <returns></returns>
		public static JsonLogEvent Parse(ReadOnlyMemory<byte> data)
		{
			JsonLogEvent logEvent;
			if (!TryParse(data, out logEvent))
			{
				throw new InvalidOperationException("Cannot parse string");
			}
			return logEvent;
		}

		/// <summary>
		/// Tries to parse a Json log event from the given 
		/// </summary>
		/// <param name="data"></param>
		/// <param name="logEvent"></param>
		/// <returns></returns>
		public static bool TryParse(ReadOnlyMemory<byte> data, out JsonLogEvent logEvent)
		{
			try
			{
				LogLevel level = LogLevel.None;
				int eventId = 0;
				int lineIndex = 0;
				int lineCount = 1;

				Utf8JsonReader reader = new Utf8JsonReader(data.Span);
				if (reader.Read() && reader.TokenType == JsonTokenType.StartObject)
				{
					while (reader.Read() && reader.TokenType == JsonTokenType.PropertyName)
					{
						ReadOnlySpan<byte> propertyName = reader.ValueSpan;
						if (!reader.Read())
						{
							break;
						}
						else if (propertyName.SequenceEqual(LogEventPropertyName.Level) && reader.TokenType == JsonTokenType.String)
						{
							level = ParseLevel(reader.ValueSpan);
						}
						else if (propertyName.SequenceEqual(LogEventPropertyName.Id) && reader.TokenType == JsonTokenType.Number)
						{
							eventId = reader.GetInt32();
						}
						else if (propertyName.SequenceEqual(LogEventPropertyName.Line) && reader.TokenType == JsonTokenType.Number)
						{
							reader.TryGetInt32(out lineIndex);
						}
						else if (propertyName.SequenceEqual(LogEventPropertyName.LineCount) && reader.TokenType == JsonTokenType.Number)
						{
							reader.TryGetInt32(out lineCount);
						}
						reader.Skip();
					}
				}

				if (reader.TokenType == JsonTokenType.EndObject && level != LogLevel.None && reader.BytesConsumed == data.Length)
				{
					logEvent = new JsonLogEvent(level, new EventId(eventId), lineIndex, lineCount, data.ToArray());
					return true;
				}
			}
			catch
			{
			}

			logEvent = default;
			return false;
		}

		static readonly sbyte[] s_firstCharToLogLevel;
		static readonly byte[][] s_logLevelNames;

		static JsonLogEvent()
		{
			const int LogLevelCount = (int)LogLevel.None;

			s_firstCharToLogLevel = new sbyte[256];
			Array.Fill(s_firstCharToLogLevel, (sbyte)-1);

			s_logLevelNames = new byte[LogLevelCount][];
			for (int idx = 0; idx < (int)LogLevel.None; idx++)
			{
				byte[] name = Encoding.UTF8.GetBytes(Enum.GetName(typeof(LogLevel), (LogLevel)idx)!);
				s_logLevelNames[idx] = name;
				s_firstCharToLogLevel[name[0]] = (sbyte)idx;
			}
		}

		static LogLevel ParseLevel(ReadOnlySpan<byte> level)
		{
			int result = s_firstCharToLogLevel[level[0]];
			if (!level.SequenceEqual(s_logLevelNames[result]))
			{
				throw new InvalidOperationException();
			}
			return (LogLevel)result;
		}

		static readonly Utf8String s_newlineEscaped = "\\n";

		/// <summary>
		/// Count the number of lines in the message field of a log event
		/// </summary>
		/// <returns>Number of lines in the message</returns>
		public int GetMessageLineCount()
		{
			if (Data.Span.IndexOf(s_newlineEscaped.Span) != -1)
			{
				Utf8JsonReader reader = new Utf8JsonReader(Data.Span);
				if (reader.Read() && reader.TokenType == JsonTokenType.StartObject)
				{
					while (reader.Read() && reader.TokenType == JsonTokenType.PropertyName)
					{
						ReadOnlySpan<byte> propertyName = reader.ValueSpan;
						if (!reader.Read())
						{
							break;
						}
						else if (propertyName.SequenceEqual(LogEventPropertyName.Message) && reader.TokenType == JsonTokenType.String)
						{
							return CountLines(reader.ValueSpan);
						}
					}
				}
			}
			return 1;
		}
		
		/// <summary>
		/// Counts the number of newlines in an escaped JSON string
		/// </summary>
		/// <param name="str">The escaped string</param>
		/// <returns></returns>
		static int CountLines(ReadOnlySpan<byte> str)
		{
			int lines = 1;
			for (int idx = 0; idx < str.Length - 1; idx++)
			{
				if (str[idx] == '\\')
				{
					if (str[idx + 1] == 'n')
					{
						lines++;
					}
					idx++;
				}
			}
			return lines;
		}

		/// <summary>
		/// Formats an event as a string
		/// </summary>
		/// <param name="state"></param>
		/// <param name="ex"></param>
		/// <returns></returns>
		public static string Format(JsonLogEvent state, Exception? ex) => LogEvent.Read(state.Data.Span).ToString();

		/// <inheritdoc/>
		public override string ToString() => Encoding.UTF8.GetString(Data.ToArray());
	}
}
