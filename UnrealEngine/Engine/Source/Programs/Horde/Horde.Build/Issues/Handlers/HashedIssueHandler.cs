// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using EpicGames.Core;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Graphs;

namespace Horde.Build.Issues.Handlers
{
	/// <summary>
	/// Instance of a particular compile error
	/// </summary>
	class HashedIssueHandler : IssueHandler
	{
		const string NodeName = "Node";

		/// <inheritdoc/>
		public override string Type => "Hashed";

		/// <inheritdoc/>
		public override int Priority => 1;

		/// <inheritdoc/>
		public override void TagEvents(IJob job, INode node, IReadOnlyNodeAnnotations annotations, IReadOnlyList<IssueEvent> stepEvents)
		{
			string[] metadata = new[] { $"{NodeName}={node.Name}" };

			NewIssueFingerprint? genericFingerprint = null;
			HashSet<Md5Hash> hashes = new HashSet<Md5Hash>();

			foreach (IssueEvent stepEvent in stepEvents)
			{
				if (hashes.Count < 25 && TryGetHash(stepEvent.Message, out Md5Hash hash))
				{
					hashes.Add(hash);
					stepEvent.Fingerprint = new NewIssueFingerprint(Type, new[] { $"hash:{hash}" }, null, metadata);
				}
				else
				{
					genericFingerprint ??= new NewIssueFingerprint(Type, new[] { $"step:{job.StreamId}:{job.TemplateId}:{node.Name}" }, null, metadata);
					stepEvent.Fingerprint = genericFingerprint;
				}
			}
		}

		static bool TryGetHash(string message, out Md5Hash hash)
		{
			string sanitized = message.ToUpperInvariant();
			sanitized = Regex.Replace(sanitized, @"0[xX][0-9a-fA-F]+", "H"); // Redact hex strings
			sanitized = Regex.Replace(sanitized, @"\d[\d.,:]*", "n"); // Redact numbers and timestamp like things

			if (sanitized.Length > 30)
			{
				hash = Md5Hash.Compute(Encoding.UTF8.GetBytes(sanitized));
				return true;
			}
			else
			{
				hash = Md5Hash.Zero;
				return false;
			}
		}

		/// <inheritdoc/>
		public override void RankSuspects(IIssueFingerprint fingerprint, List<SuspectChange> suspects)
		{
		}

		/// <inheritdoc/>
		public override string GetSummary(IIssueFingerprint fingerprint, IssueSeverity severity)
		{
			string severityText = (severity == IssueSeverity.Warning) ? "Warnings" : "Errors";
			string[] nodeNames = fingerprint.GetMetadataValues(NodeName).ToArray();
			return $"{severityText} in {StringUtils.FormatList(nodeNames, 2)}";
		}
	}
}
