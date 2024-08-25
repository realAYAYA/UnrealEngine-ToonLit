// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Text.RegularExpressions;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

#nullable enable

namespace AutomationUtils.Matchers
{
	/// <summary>
	/// Matches shader compile errors and annotates with the source file path and revision
	/// </summary>
	class ShaderEventMatcher : ILogEventMatcher
	{
		const string Prefix = @"^\s*LogShaderCompilers: (?<severity>Error|Warning): ";

		const string FilePattern =
			@"(?<file>" +
				// optional drive letter
				@"(?:[a-zA-Z]:)?" +
				// any non-colon character
				@"[^:(\s]+" +
				// any filename character (not whitespace or slash)
				@"[^:(\s\\/]" +
			@")";

		const string LinePattern =
			@"(?<line>\d+)(?::(?<column>\d+))?";

		static readonly Regex s_pattern = new Regex(Prefix);
		static readonly Regex s_patternWithFile = new Regex($"{Prefix}{FilePattern}(?:\\({LinePattern}\\))?:");
		static readonly Regex s_content = new Regex(@"^(?:\s+|Validation failed)");

		/// <inheritdoc/>
		public LogEventMatch? Match(ILogCursor input)
		{
			Match? match;
			if (input.TryMatch(s_pattern, out match))
			{
				LogLevel level = GetLogLevelFromSeverity(match);

				LogEventBuilder builder = new LogEventBuilder(input);
				builder.Annotate(match.Groups["severity"], LogEventMarkup.Severity);

				Match? fileMatch;
				if (input.TryMatch(s_patternWithFile, out fileMatch))
				{
					builder.AnnotateSourceFile(fileMatch.Groups["file"], "");
					builder.TryAnnotate(fileMatch.Groups["line"], LogEventMarkup.LineNumber);
					builder.TryAnnotate(fileMatch.Groups["column"], LogEventMarkup.ColumnNumber);
				}

				while (builder.Next.IsMatch(s_content))
				{
					builder.MoveNext();
				}

				return builder.ToMatch(LogEventPriority.AboveNormal, level, KnownLogEvents.Engine_ShaderCompiler);
			}
			return null;
		}

		static LogLevel GetLogLevelFromSeverity(Match match)
		{
			string severity = match.Groups["severity"].Value;
			if (severity.Equals("Warning", StringComparison.Ordinal))
			{
				return LogLevel.Warning;
			}
			else
			{
				return LogLevel.Error;
			}
		}
	}
}
