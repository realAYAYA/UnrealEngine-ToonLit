// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Graphs;

namespace Horde.Build.Issues
{
	/// <summary>
	/// Interface for issue matchers
	/// </summary>
	abstract class IssueHandler
	{
		/// <summary>
		/// Identifier for the type of issue
		/// </summary>
		public abstract string Type { get; }

		/// <summary>
		/// Priority of this matcher
		/// </summary>
		public abstract int Priority { get; }

		/// <summary>
		/// Tag all thethe events for a step completing
		/// </summary>
		/// <param name="job">The job that spawned the event</param>
		/// <param name="node">Node that was executed</param>
		/// <param name="annotations">Combined annotations from this step</param>
		/// <param name="events">Events from this step</param>
		public abstract void TagEvents(IJob job, INode node, IReadOnlyNodeAnnotations annotations, IReadOnlyList<IssueEvent> events);

		/// <summary>
		/// Rank all the suspect changes for a given fingerprint
		/// </summary>
		/// <param name="fingerprint">The issue fingerprint</param>
		/// <param name="suspects">Potential suspects</param>
		public abstract void RankSuspects(IIssueFingerprint fingerprint, List<SuspectChange> suspects);

		/// <summary>
		/// Gets the summary text for an issue
		/// </summary>
		/// <param name="fingerprint">The fingerprint</param>
		/// <param name="severity">Severity of the issue</param>
		/// <returns>The summary text</returns>
		public abstract string GetSummary(IIssueFingerprint fingerprint, IssueSeverity severity);
	}
}
