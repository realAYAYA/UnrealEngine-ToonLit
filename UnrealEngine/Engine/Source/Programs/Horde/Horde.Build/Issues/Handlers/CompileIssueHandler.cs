// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using EpicGames.Core;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Logs;
using Microsoft.Extensions.Logging;
using MongoDB.Driver;

namespace Horde.Build.Issues.Handlers
{
	/// <summary>
	/// Instance of a particular compile error
	/// </summary>
	class CompileIssueHandler : SourceFileIssueHandler
	{
		/// <summary>
		/// Annotation describing the compile type
		/// </summary>
		const string CompileTypeAnnotation = "CompileType";

		/// <summary>
		/// Annotation specifying a group for compile issues from this node
		/// </summary>
		const string CompileGroupAnnotation = "CompileGroup";

		/// <inheritdoc/>
		public override string Type => "Compile";

		/// <inheritdoc/>
		public override int Priority => 10;

		/// <summary>
		/// Determines if the given event id matches
		/// </summary>
		/// <param name="eventId">The event id to compare</param>
		/// <returns>True if the given event id matches</returns>
		public static bool IsMatchingEventId(EventId eventId)
		{
			return eventId == KnownLogEvents.Compiler || eventId == KnownLogEvents.AutomationTool_SourceFileLine || eventId == KnownLogEvents.MSBuild;
		}

		static bool IsMaskedEventId(EventId id) => id == KnownLogEvents.Generic || id == KnownLogEvents.ExitCode || id == KnownLogEvents.Systemic_Xge_BuildFailed;

		/// <inheritdoc/>
		public override void TagEvents(IJob job, INode node, IReadOnlyNodeAnnotations annotations, IReadOnlyList<IssueEvent> stepEvents)
		{
			bool hasMatches = false;
			foreach (IssueEvent stepEvent in stepEvents)
			{
				if (stepEvent.EventId.HasValue)
				{
					EventId eventId = stepEvent.EventId.Value;
					if (IsMatchingEventId(eventId))
					{
						List<string> newFileNames = new List<string>();
						GetSourceFiles(stepEvent.EventData, newFileNames);

						string compileType = "Compile";
						if (annotations.TryGetValue(CompileTypeAnnotation, out string? type))
						{
							compileType = type;
						}

						string fingerprintType = Type;
						if (annotations.TryGetValue(CompileGroupAnnotation, out string? group))
						{
							fingerprintType = $"{fingerprintType}:{group}";
						}

						List<string> newMetadata = new List<string>();
						newMetadata.Add($"{CompileTypeAnnotation}={compileType}");

						stepEvent.Fingerprint = new NewIssueFingerprint(fingerprintType, newFileNames, null, newMetadata);
						hasMatches = true;
					}
					else if (hasMatches && IsMaskedEventId(eventId))
					{
						stepEvent.Ignored = true;
					}
				}
			}
		}

		/// <inheritdoc/>
		public override string GetSummary(IIssueFingerprint fingerprint, IssueSeverity severity)
		{
			List<string> types = fingerprint.GetMetadataValues(CompileTypeAnnotation).ToList();
			string type = (types.Count == 1) ? types[0] : "Compile";
			string level = (severity == IssueSeverity.Warning) ? "warnings" : "errors";
			string list = StringUtils.FormatList(fingerprint.Keys.Where(x => !x.StartsWith(NotePrefix, StringComparison.Ordinal)).ToArray(), 2);
			return $"{type} {level} in {list}";
		}
	}
}
