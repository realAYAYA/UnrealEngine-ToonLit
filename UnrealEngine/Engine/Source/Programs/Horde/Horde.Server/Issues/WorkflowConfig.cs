// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using EpicGames.Horde.Issues;
using Horde.Server.Jobs.Graphs;

namespace Horde.Server.Issues
{
	/// <summary>
	/// Configuration for an issue workflow
	/// </summary>
	public class WorkflowConfig
	{
		/// <summary>
		/// Identifier for this workflow
		/// </summary>
		public WorkflowId Id { get; set; } = WorkflowId.Empty;

		/// <summary>
		/// Times of day at which to send a report
		/// </summary>
		public List<TimeSpan> ReportTimes { get; set; } = new List<TimeSpan>();

		/// <summary>
		/// Name of the tab to post summary data to
		/// </summary>
		public string? SummaryTab { get; set; }

		/// <summary>
		/// Channel to post summary information for these templates.
		/// </summary>
		public string? ReportChannel { get; set; }

		/// <summary>
		/// Whether to group issues by template in the report
		/// </summary>
		public bool GroupIssuesByTemplate { get; set; } = true;

		/// <summary>
		/// Channel to post threads for triaging new issues
		/// </summary>
		public string? TriageChannel { get; set; }

		/// <summary>
		/// Prefix for all triage messages
		/// </summary>
		public string? TriagePrefix { get; set; } = "*[NEW]* ";

		/// <summary>
		/// Suffix for all triage messages
		/// </summary>
		public string? TriageSuffix { get; set; }

		/// <summary>
		/// Instructions posted to triage threads
		/// </summary>
		public string? TriageInstructions { get; set; }

		/// <summary>
		/// User id of a Slack user/alias to ping if there is nobody assigned to an issue by default.
		/// </summary>
		public string? TriageAlias { get; set; }

		/// <summary>
		/// Slack user/alias to ping for specific issue types (such as Systemic), if there is nobody assigned to an issue by default.
		/// </summary>
		public Dictionary<string, string>? TriageTypeAliases { get; set; }

		/// <summary>
		/// Alias to ping if an issue has not been resolved for a certain amount of time
		/// </summary>
		public string? EscalateAlias { get; set; }

		/// <summary>
		/// Times after an issue has been opened to escalate to the alias above, in minutes. Continues to notify on the last interval once reaching the end of the list.
		/// </summary>
		public List<int> EscalateTimes { get; set; } = new List<int> { 120 };

		/// <summary>
		/// Maximum number of people to mention on a triage thread
		/// </summary>
		public int MaxMentions { get; set; } = 5;

		/// <summary>
		/// Whether to mention people on this thread. Useful to disable for testing.
		/// </summary>
		public bool AllowMentions { get; set; } = true;

		/// <summary>
		/// Uses the admin.conversations.invite API to invite users to the channel
		/// </summary>
		public bool InviteRestrictedUsers { get; set; }

		/// <summary>
		/// Skips sending reports when there are no active issues. 
		/// </summary>
		public bool SkipWhenEmpty { get; set; }

		/// <summary>
		/// Additional node annotations implicit in this workflow
		/// </summary>
		public NodeAnnotations Annotations { get; set; } = new NodeAnnotations();

		/// <summary>
		/// External issue tracking configuration for this workflow
		/// </summary>
		public ExternalIssueConfig? ExternalIssues { get; set; }

		/// <summary>
		/// Additional issue handlers enabled for this workflow
		/// </summary>
		public List<string>? IssueHandlers { get; set; }
	}

	/// <summary>
	/// External issue tracking configuration for a workflow
	/// </summary>
	public class ExternalIssueConfig
	{
		/// <summary>
		/// Project key in external issue tracker
		/// </summary>
		[Required]
		public string ProjectKey { get; set; } = String.Empty;

		/// <summary>
		/// Default component id for issues using workflow
		/// </summary>
		[Required]
		public string DefaultComponentId { get; set; } = String.Empty;

		/// <summary>
		/// Default issue type id for issues using workflow
		/// </summary>
		[Required]
		public string DefaultIssueTypeId { get; set; } = String.Empty;
	}
}
