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
	/// Instance of a particular shader compile error
	/// </summary>
	class ShaderIssueHandler : SourceFileIssueHandler
	{
		/// <inheritdoc/>
		public override string Type => "Shader";

		/// <inheritdoc/>
		public override int Priority => 10;

		/// <summary>
		/// Determines if the given event id matches
		/// </summary>
		/// <param name="eventId">The event id to compare</param>
		/// <returns>True if the given event id matches</returns>
		public static bool IsMatchingEventId(EventId eventId)
		{
			return eventId == KnownLogEvents.Engine_ShaderCompiler;
		}

		static bool IsMaskedEventId(EventId id) => id == KnownLogEvents.Generic || id == KnownLogEvents.ExitCode || id == KnownLogEvents.Systemic_Xge_BuildFailed || id == KnownLogEvents.Engine_Crash || id == KnownLogEvents.AutomationTool_CrashExitCode || id == KnownLogEvents.Engine_AppError;

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

						stepEvent.Fingerprint = new NewIssueFingerprint(Type, newFileNames, null, null);
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
			string level = (severity == IssueSeverity.Warning) ? "warnings" : "errors";
			string list = StringUtils.FormatList(fingerprint.Keys.Where(x => !x.StartsWith(NotePrefix, StringComparison.Ordinal)).ToArray(), 2);
			return $"Shader compile {level} in {list}";
		}
	}
}
