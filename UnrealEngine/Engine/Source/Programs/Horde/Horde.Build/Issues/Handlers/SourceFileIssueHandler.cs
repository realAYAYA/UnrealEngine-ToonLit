// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Text.Json;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Logs;
using Horde.Build.Utilities;
using MongoDB.Driver;

namespace Horde.Build.Issues.Handlers
{
	/// <summary>
	/// Instance of a particular compile error
	/// </summary>
	abstract class SourceFileIssueHandler : IssueHandler
	{
		/// <summary>
		/// Prefix used to identify files that may match against modified files, but which are not the files failing to compile
		/// </summary>
		protected const string NotePrefix = "note:";

		/// <summary>
		/// Extracts a list of source files from an event
		/// </summary>
		/// <param name="logEventData">The event data</param>
		/// <param name="sourceFiles">List of source files</param>
		protected static void GetSourceFiles(ILogEventData logEventData, List<string> sourceFiles)
		{
			foreach (ILogEventLine line in logEventData.Lines)
			{
				JsonElement properties;
				if (line.Data.TryGetProperty("properties", out properties) && properties.ValueKind == JsonValueKind.Object)
				{
					string? prefix = null;

					JsonElement noteElement;
					if (properties.TryGetProperty("note", out noteElement) && noteElement.GetBoolean())
					{
						prefix = NotePrefix;
					}

					foreach (JsonProperty property in properties.EnumerateObject())
					{
						if (property.NameEquals("file") && property.Value.ValueKind == JsonValueKind.String)
						{
							AddSourceFile(sourceFiles, property.Value.GetString()!, prefix);
						}
						if (property.Value.HasStringProperty("$type", "SourceFile") && property.Value.TryGetStringProperty("relativePath", out string? value))
						{
							AddSourceFile(sourceFiles, value, prefix);
						}
					}
				}
			}
		}

		/// <summary>
		/// Add a new source file to a list of unique source files
		/// </summary>
		/// <param name="sourceFiles">List of source files</param>
		/// <param name="relativePath">File to add</param>
		/// <param name="prefix">Prefix to insert at the start of the filename</param>
		static void AddSourceFile(List<string> sourceFiles, string relativePath, string? prefix)
		{
			int endIdx = relativePath.LastIndexOfAny(new char[] { '/', '\\' }) + 1;

			string fileName = relativePath.Substring(endIdx);
			if (prefix != null)
			{
				fileName = prefix + fileName;
			}

			if (!sourceFiles.Any(x => x.Equals(fileName, StringComparison.OrdinalIgnoreCase)))
			{
				sourceFiles.Add(fileName);
			}
		}

		/// <inheritdoc/>
		public override void RankSuspects(IIssueFingerprint fingerprint, List<SuspectChange> suspects)
		{
			List<string> fileNames = new List<string>();
			foreach (string key in fingerprint.Keys)
			{
				if (key.StartsWith(NotePrefix, StringComparison.Ordinal))
				{
					fileNames.Add(key.Substring(NotePrefix.Length));
				}
				else
				{
					fileNames.Add(key);
				}
			}

			foreach (SuspectChange change in suspects)
			{
				if (change.ContainsCode)
				{
					if (fileNames.Any(x => change.ModifiesFile(x)))
					{
						change.Rank += 20;
					}
					else
					{
						change.Rank += 10;
					}
				}
			}
		}
	}
}
