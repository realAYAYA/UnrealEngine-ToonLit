// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Graphs;

namespace Horde.Build.Issues.Handlers
{
	/// <summary>
	/// Instance of a particular compile error
	/// </summary>
	class DefaultIssueHandler : IssueHandler
	{
		/// <summary>
		/// Name of the handler
		/// </summary>
		public const string TypeConst = "Default";

		/// <inheritdoc/>
		public override string Type => TypeConst;

		/// <inheritdoc/>
		public override int Priority => 0;

		/// <inheritdoc/>
		public override void TagEvents(IJob job, INode node, IReadOnlyNodeAnnotations annotations, IReadOnlyList<IssueEvent> stepEvents)
		{
			NewIssueFingerprint fingerprint = new NewIssueFingerprint(TypeConst, new[] { node.Name }, null, null);
			foreach (IssueEvent stepEvent in stepEvents)
			{
				stepEvent.Fingerprint = fingerprint;
			}
		}

		/// <inheritdoc/>
		public override void RankSuspects(IIssueFingerprint fingerprint, List<SuspectChange> suspects)
		{
		}

		/// <inheritdoc/>
		public override string GetSummary(IIssueFingerprint fingerprint, IssueSeverity severity)
		{
			string nodeName = fingerprint.Keys.FirstOrDefault() ?? "(unknown)";
			if(severity == IssueSeverity.Warning)
			{
				return $"Warnings in {nodeName}";
			}
			else
			{
				return $"Errors in {nodeName}";
			}
		}
	}
}
