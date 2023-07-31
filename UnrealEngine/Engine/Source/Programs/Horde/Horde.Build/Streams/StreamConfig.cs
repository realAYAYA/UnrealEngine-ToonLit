// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using EpicGames.Core;
using Horde.Build.Acls;
using Horde.Build.Issues;
using Horde.Build.Utilities;

namespace Horde.Build.Streams
{
	using TemplateRefId = StringId<TemplateRef>;
	using WorkflowId = StringId<WorkflowConfig>;

	/// <summary>
	/// Config for a stream
	/// </summary>
	[JsonSchema("https://unrealengine.com/horde/stream")]
	[JsonSchemaCatalog("Horde Stream", "Horde stream configuration file", new[] { "*.stream.json", "Streams/*.json" })]
	public class StreamConfig
	{
		/// <summary>
		/// Name of the stream
		/// </summary>
		[Required]
		public string Name { get; set; } = String.Empty;

		/// <summary>
		/// The perforce cluster containing the stream
		/// </summary>
		public string? ClusterName { get; set; }

		/// <summary>
		/// Order for this stream
		/// </summary>
		public int? Order { get; set; }

		/// <summary>
		/// Notification channel for all jobs in this stream
		/// </summary>
		public string? NotificationChannel { get; set; }

		/// <summary>
		/// Notification channel filter for this template. Can be Success|Failure|Warnings
		/// </summary>
		public string? NotificationChannelFilter { get; set; }

		/// <summary>
		/// Channel to post issue triage notifications
		/// </summary>
		public string? TriageChannel { get; set; }

		/// <summary>
		/// Default template for running preflights
		/// </summary>
		public string? DefaultPreflightTemplate { get; set; }

		/// <summary>
		/// Default template for running preflights
		/// </summary>
		public DefaultPreflightRequest? DefaultPreflight { get; set; }

		/// <summary>
		/// List of tabs to show for the new stream
		/// </summary>
		public List<CreateStreamTabRequest> Tabs { get; set; } = new List<CreateStreamTabRequest>();

		/// <summary>
		/// Map of agent name to type
		/// </summary>
		public Dictionary<string, CreateAgentTypeRequest> AgentTypes { get; set; } = new Dictionary<string, CreateAgentTypeRequest>();

		/// <summary>
		/// Map of workspace name to type
		/// </summary>
		public Dictionary<string, CreateWorkspaceTypeRequest> WorkspaceTypes { get; set; } = new Dictionary<string, CreateWorkspaceTypeRequest>();

		/// <summary>
		/// List of templates to create
		/// </summary>
		public List<TemplateRefConfig> Templates { get; set; } = new List<TemplateRefConfig>();

		/// <summary>
		/// Custom permissions for this object
		/// </summary>
		public UpdateAclRequest? Acl { get; set; }

		/// <summary>
		/// Pause stream builds until specified date
		/// </summary>
		public DateTime? PausedUntil { get; set; }

		/// <summary>
		/// Reason for pausing builds of the stream
		/// </summary>
		public string? PauseComment { get; set; }

		/// <summary>
		/// How to replicate data from VCS to Horde Storage.
		/// </summary>
		public ContentReplicationMode ReplicationMode { get; set; }

		/// <summary>
		/// Filter for paths to be replicated to storage, as a Perforce wildcard relative to the root of the workspace.
		/// </summary>
		public string? ReplicationFilter { get; set; }

		/// <summary>
		/// Stream to use for replication, if different to the default.
		/// </summary>
		public string? ReplicationStream { get; set; }

		/// <summary>
		/// Workflows for dealing with new issues
		/// </summary>
		public List<WorkflowConfig> Workflows { get; set; } = new List<WorkflowConfig>();

		/// <summary>
		/// Tokens to create for each job step
		/// </summary>
		public List<TokenConfig> Tokens { get; set; } = new List<TokenConfig>(); 

		/// <summary>
		/// Tries to find a template with the given id
		/// </summary>
		/// <param name="templateRefId"></param>
		/// <param name="templateConfig"></param>
		/// <returns></returns>
		public bool TryGetTemplate(TemplateRefId templateRefId, [NotNullWhen(true)] out TemplateRefConfig? templateConfig)
		{
			templateConfig = Templates.FirstOrDefault(x => x.Id == templateRefId);
			return templateConfig != null;
		}

		/// <summary>
		/// Tries to find a workflow with the given id
		/// </summary>
		/// <param name="workflowId"></param>
		/// <param name="workflowConfig"></param>
		/// <returns></returns>
		public bool TryGetWorkflow(WorkflowId workflowId, [NotNullWhen(true)] out WorkflowConfig? workflowConfig)
		{
			workflowConfig = Workflows.FirstOrDefault(x => x.Id == workflowId);
			return workflowConfig != null;
		}
	}

	/// <summary>
	/// Configuration for allocating access tokens for each job
	/// </summary>
	public class TokenConfig
	{
		/// <summary>
		/// URL to request tokens from
		/// </summary>
		[Required]
		public Uri Url { get; set; } = null!;

		/// <summary>
		/// Client id to use to request a new token
		/// </summary>
		[Required]
		public string ClientId { get; set; } = String.Empty;

		/// <summary>
		/// Client secret to request a new access token
		/// </summary>
		[Required]
		public string ClientSecret { get; set; } = String.Empty;

		/// <summary>
		/// Environment variable to set with the access token
		/// </summary>
		[Required]
		public string EnvVar { get; set; } = String.Empty;
	}
}
