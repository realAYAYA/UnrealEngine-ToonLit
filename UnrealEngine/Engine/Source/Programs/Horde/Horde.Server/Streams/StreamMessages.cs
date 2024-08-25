// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using EpicGames.Core;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Jobs.Templates;
using EpicGames.Horde.Projects;
using EpicGames.Horde.Streams;
using EpicGames.Horde.Users;
using Horde.Server.Issues;
using Horde.Server.Jobs.Schedules;
using Horde.Server.Jobs.Templates;
using Horde.Server.Perforce;
using Horde.Server.Projects;
using Horde.Server.Users;

namespace Horde.Server.Streams
{
	/// <summary>
	/// Step state update request
	/// </summary>
	public class UpdateStepStateRequest
	{
		/// <summary>
		/// Name of the step
		/// </summary>
		[Required]
		public string Name { get; set; } = String.Empty;

		/// <summary>
		/// User who paused the step
		/// </summary>
		public string? PausedByUserId { get; set; }
	}

	/// <summary>
	/// Updates an existing stream template ref
	/// </summary>
	public class UpdateTemplateRefRequest
	{
		/// <summary>
		/// Step states to update
		/// </summary>
		public List<UpdateStepStateRequest>? StepStates { get; set; }
	}

	/// <summary>
	/// Mapping from a BuildGraph agent type to a set of machines on the farm
	/// </summary>
	public class GetAgentTypeResponse
	{
		/// <summary>
		/// Pool of agents to use for this agent type
		/// </summary>
		public string Pool { get; set; }

		/// <summary>
		/// Name of the workspace to sync
		/// </summary>
		public string? Workspace { get; set; }

		/// <summary>
		/// Path to the temporary storage dir
		/// </summary>
		public string? TempStorageDir { get; set; }

		/// <summary>
		/// Environment variables to be set when executing the job
		/// </summary>
		public Dictionary<string, string>? Environment { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="pool">Pool of agents to use for this agent type</param>
		/// <param name="workspace">Name of the workspace to sync</param>
		/// <param name="tempStorageDir">Path to the temp storage directory</param>
		/// <param name="environment">Environment variables to be set when executing this job</param>
		public GetAgentTypeResponse(string pool, string? workspace, string? tempStorageDir, Dictionary<string, string>? environment)
		{
			Pool = pool;
			Workspace = workspace;
			TempStorageDir = tempStorageDir;
			Environment = environment;
		}
	}

	/// <summary>
	/// Information about a workspace type
	/// </summary>
	public class GetWorkspaceTypeResponse
	{
		/// <summary>
		/// The Perforce server cluster
		/// </summary>
		public string? Cluster { get; set; }

		/// <summary>
		/// The Perforce server and port (eg. perforce:1666)
		/// </summary>
		public string? ServerAndPort { get; set; }

		/// <summary>
		/// User to log into Perforce with (defaults to buildmachine)
		/// </summary>
		public string? UserName { get; set; }

		/// <summary>
		/// Identifier to distinguish this workspace from other workspaces. Defaults to the workspace type name.
		/// </summary>
		public string? Identifier { get; set; }

		/// <summary>
		/// Override for the stream to sync
		/// </summary>
		public string? Stream { get; set; }

		/// <summary>
		/// Custom view for the workspace
		/// </summary>
		public List<string>? View { get; set; }

		/// <summary>
		/// Whether to use an incrementally synced workspace
		/// </summary>
		public bool Incremental { get; set; }

		/// <summary>
		/// Whether to use the AutoSDK
		/// </summary>
		public bool UseAutoSdk { get; set; }

		/// <summary>
		/// Default constructor
		/// </summary>
		public GetWorkspaceTypeResponse()
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="cluster">The server cluster</param>
		/// <param name="serverAndPort">The perforce server</param>
		/// <param name="userName">The perforce user name</param>
		/// <param name="identifier">Identifier to distinguish this workspace from other workspaces. Defaults to the workspace type name.</param>
		/// <param name="stream">Override for the stream to sync</param>
		/// <param name="view">Custom view for the workspace</param>
		/// <param name="incremental">Whether to use an incrementally synced workspace</param>
		/// <param name="useAutoSdk">Whether to use the AutoSDK</param>
		public GetWorkspaceTypeResponse(string? cluster, string? serverAndPort, string? userName, string? identifier, string? stream, List<string>? view, bool incremental, bool useAutoSdk)
		{
			Cluster = cluster;
			ServerAndPort = serverAndPort;
			UserName = userName;
			Identifier = identifier;
			Stream = stream;
			View = view;
			Incremental = incremental;
			UseAutoSdk = useAutoSdk;
		}
	}

	/// <summary>
	/// State information for a step in the stream
	/// </summary>
	public class GetTemplateStepStateResponse
	{
		/// <summary>
		/// Name of the step
		/// </summary>
		public string Name { get; set; } = String.Empty;

		/// <summary>
		/// User who paused the step
		/// </summary>
		public GetThinUserInfoResponse? PausedByUserInfo { get; set; }

		/// <summary>
		/// The UTC time when the step was paused
		/// </summary>
		public DateTime? PauseTimeUtc { get; set; }

		/// <summary>
		/// Default constructor for serialization
		/// </summary>
		private GetTemplateStepStateResponse()
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public GetTemplateStepStateResponse(ITemplateStep state, GetThinUserInfoResponse? pausedByUserInfo)
		{
			Name = state.Name;
			PauseTimeUtc = state.PauseTimeUtc;
			PausedByUserInfo = pausedByUserInfo;
		}
	}

	/// <summary>
	/// Information about a template in this stream
	/// </summary>
	public class GetTemplateRefResponse : GetTemplateResponseBase
	{
		/// <summary>
		/// Id of the template ref
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// Hash of the template definition
		/// </summary>
		public string Hash { get; set; }

		/// <summary>
		/// Whether to show badges in UGS for these jobs
		/// </summary>
		public bool ShowUgsBadges { get; set; }

		/// <summary>
		/// Whether to show alerts in UGS for these jobs
		/// </summary>
		public bool ShowUgsAlerts { get; set; }

		/// <summary>
		/// Notification channel for this template. Overrides the stream channel if set.
		/// </summary>
		public string? NotificationChannel { get; set; }

		/// <summary>
		/// Notification channel filter for this template. Can be a combination of "Success", "Failure" and "Warnings" separated by pipe characters.
		/// </summary>
		public string? NotificationChannelFilter { get; set; }

		/// <summary>
		/// Triage channel for this template. Overrides the stream channel if set.
		/// </summary>
		public string? TriageChannel { get; set; }

		/// <summary>
		/// The schedule for this ref
		/// </summary>
		public GetScheduleResponse? Schedule { get; set; }

		/// <summary>
		/// List of templates to trigger
		/// </summary>
		public List<ChainedJobTemplateConfig>? ChainedJobs { get; set; }

		/// <summary>
		/// List of step states
		/// </summary>
		public List<GetTemplateStepStateResponse>? StepStates { get; set; }

		/// <summary>
		/// List of queries for the default changelist
		/// </summary>
		public List<ChangeQueryConfig>? DefaultChange { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="id">The template ref id</param>
		/// <param name="templateRef">The template ref</param>
		/// <param name="template">The actual template</param>
		/// <param name="stepStates">The template step states</param>
		/// <param name="schedulerTimeZone">The scheduler time zone</param>
		public GetTemplateRefResponse(TemplateId id, ITemplateRef templateRef, ITemplate template, List<GetTemplateStepStateResponse>? stepStates, TimeZoneInfo schedulerTimeZone)
			: base(template)
		{
			Id = id.ToString();
			Hash = template.Hash.ToString();
			ShowUgsBadges = templateRef.Config.ShowUgsBadges;
			ShowUgsAlerts = templateRef.Config.ShowUgsAlerts;
			NotificationChannel = templateRef.Config.NotificationChannel;
			NotificationChannelFilter = templateRef.Config.NotificationChannelFilter;
			Schedule = (templateRef.Schedule != null) ? new GetScheduleResponse(templateRef.Schedule, schedulerTimeZone) : null;
			ChainedJobs = (templateRef.Config.ChainedJobs != null && templateRef.Config.ChainedJobs.Count > 0) ? templateRef.Config.ChainedJobs : null;
			StepStates = stepStates;
			DefaultChange = templateRef.Config.DefaultChange;
		}
	}

	/// <summary>
	/// Response describing a stream
	/// </summary>
	public class GetStreamResponse
	{
		/// <summary>
		/// Unique id of the stream
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// Unique id of the project containing this stream
		/// </summary>
		public string ProjectId { get; set; }

		/// <summary>
		/// Name of the stream
		/// </summary>
		public string Name { get; set; }

		/// <summary>
		/// The config file path on the server
		/// </summary>
		public string ConfigPath { get; set; } = String.Empty;

		/// <summary>
		/// Revision of the config file 
		/// </summary>
		public string ConfigRevision { get; set; } = String.Empty;

		/// <summary>
		/// Order to display in the list
		/// </summary>
		public int Order { get; set; }

		/// <summary>
		/// Notification channel for all jobs in this stream
		/// </summary>
		public string? NotificationChannel { get; set; }

		/// <summary>
		/// Notification channel filter for this template. Can be a combination of "Success", "Failure" and "Warnings" separated by pipe characters.
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
		/// Default template to use for preflights
		/// </summary>
		public DefaultPreflightConfig? DefaultPreflight { get; set; }

		/// <summary>
		/// List of tabs to display for this stream
		/// </summary>
		public List<TabConfig> Tabs { get; set; }

		/// <summary>
		/// Map of agent name to type
		/// </summary>
		public Dictionary<string, AgentConfig> AgentTypes { get; set; }

		/// <summary>
		/// Map of workspace name to type
		/// </summary>
		public Dictionary<string, WorkspaceConfig>? WorkspaceTypes { get; set; }

		/// <summary>
		/// Templates for jobs in this stream
		/// </summary>
		public List<GetTemplateRefResponse> Templates { get; set; }

		/// <summary>
		/// Stream paused for new builds until this date
		/// </summary>
		public DateTime? PausedUntil { get; set; }

		/// <summary>
		/// Reason for stream being paused
		/// </summary>
		public string? PauseComment { get; set; }

		/// <summary>
		/// Workflows for this stream
		/// </summary>
		public List<WorkflowConfig> Workflows { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="stream">The stream to construct from</param>
		/// <param name="templates">Templates for this stream</param>
		public GetStreamResponse(IStream stream, List<GetTemplateRefResponse> templates)
		{
			Id = stream.Id.ToString();
			ProjectId = stream.Config.ProjectConfig.Id.ToString();
			Name = stream.Config.Name;
			ConfigPath = stream.Config.Path ?? String.Empty;
			ConfigRevision = stream.Config.Revision;
			Order = stream.Config.Order;
			NotificationChannel = stream.Config.NotificationChannel;
			NotificationChannelFilter = stream.Config.NotificationChannelFilter;
			TriageChannel = stream.Config.TriageChannel;
			DefaultPreflightTemplate = stream.Config.DefaultPreflight?.TemplateId?.ToString();
			DefaultPreflight = stream.Config.DefaultPreflight;
			Tabs = stream.Config.Tabs;
			AgentTypes = stream.Config.AgentTypes;
			WorkspaceTypes = stream.Config.WorkspaceTypes;
			Templates = templates;
			PausedUntil = stream.PausedUntil;
			PauseComment = stream.PauseComment;
			Workflows = stream.Config.Workflows;
		}
	}

	/// <summary>
	/// Response describing a stream
	/// </summary>
	public class GetStreamResponseV2
	{
		readonly IStream _stream;

		/// <inheritdoc cref="IStream.Id"/>
		public StreamId Id => _stream.Id;

		/// <inheritdoc cref="ProjectConfig.Id"/>
		public ProjectId ProjectId => _stream.Config.ProjectConfig.Id;

		/// <inheritdoc cref="StreamConfig.Name"/>
		public string Name => _stream.Config.Name;

		/// <inheritdoc cref="StreamConfig.Revision"/>
		public string ConfigRevision => _stream.Config.Revision;

		/// <inheritdoc cref="IStream.Config"/>
		public StreamConfig? Config { get; }

		/// <inheritdoc cref="IStream.Templates"/>
		public List<GetTemplateRefResponseV2> Templates { get; }

		/// <inheritdoc cref="IStream.PausedUntil"/>
		public DateTime? PausedUntil => _stream.PausedUntil;

		/// <inheritdoc cref="IStream.PauseComment"/>
		public string? PauseComment => _stream.PauseComment;

		internal GetStreamResponseV2(IStream stream, bool includeConfig, List<GetTemplateRefResponseV2> templates)
		{
			_stream = stream;

			Templates = templates;

			if (includeConfig)
			{
				Config = stream.Config;
			}
		}
	}

	/// <summary>
	/// Job template in a stream
	/// </summary>
	public class GetTemplateRefResponseV2
	{
		readonly ITemplate _template;
		readonly ITemplateRef _templateRef;

		/// <inheritdoc cref="ITemplateRef.Id"/>
		public TemplateId Id => _templateRef.Id;

		/// <inheritdoc cref="ITemplate.Hash"/>
		public ContentHash Hash => _template.Hash;

		internal GetTemplateRefResponseV2(ITemplate template, ITemplateRef templateRef)
		{
			_template = template;
			_templateRef = templateRef;
		}
	}

	/// <summary>
	/// Schedule for a template
	/// </summary>
	public class GetTemplateScheduleResponseV2
	{
		readonly ITemplateSchedule _schedule;

		/// <inheritdoc cref="ITemplateSchedule.LastTriggerChange"/>
		public int LastTriggerChange => _schedule.LastTriggerChange;

		/// <inheritdoc cref="ITemplateSchedule.LastTriggerTimeUtc"/>
		public DateTime LastTriggerTimeUtc => _schedule.LastTriggerTimeUtc;

		/// <inheritdoc cref="ITemplateSchedule.LastTriggerTimeUtc"/>
		public IReadOnlyList<JobId> ActiveJobs => _schedule.ActiveJobs;

		internal GetTemplateScheduleResponseV2(ITemplateSchedule schedule)
		{
			_schedule = schedule;
		}
	}

	/// <summary>
	/// State information for a step in the stream
	/// </summary>
	public class GetTemplateStepResponseV2
	{
		readonly ITemplateStep _step;

		/// <inheritdoc cref="ITemplateStep.Name"/>
		public string Name => _step.Name;

		/// <inheritdoc cref="ITemplateStep.PausedByUserId"/>
		public GetThinUserInfoResponse PausedByUserInfo { get; }

		/// <inheritdoc cref="ITemplateStep.PauseTimeUtc"/>
		public DateTime PauseTimeUtc => _step.PauseTimeUtc;

		/// <summary>
		/// Constructor
		/// </summary>
		public GetTemplateStepResponseV2(ITemplateStep step, GetThinUserInfoResponse pausedByUserInfo)
		{
			_step = step;
			PausedByUserInfo = pausedByUserInfo;
		}
	}

	/// <summary>
	/// Information about a commit
	/// </summary>
	public class GetCommitResponse
	{
		/// <summary>
		/// The source changelist number
		/// </summary>
		public int Number { get; set; }

		/// <summary>
		/// Name of the user that authored this change [DEPRECATED]
		/// </summary>
		public string Author { get; set; }

		/// <summary>
		/// Information about the user that authored this change
		/// </summary>
		public GetThinUserInfoResponse AuthorInfo { get; set; }

		/// <summary>
		/// The description text
		/// </summary>
		public string Description { get; set; }

		/// <summary>
		/// Tags for this commit
		/// </summary>
		public List<CommitTag>? Tags { get; set; }

		/// <summary>
		/// List of files that were modified, relative to the stream base
		/// </summary>
		public List<string>? Files { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="commit">The commit to construct from</param>
		/// <param name="author">Author of the change</param>
		/// <param name="tags">Tags for the commit</param>
		/// <param name="files">Files modified by the commit</param>
		public GetCommitResponse(ICommit commit, IUser author, IReadOnlyList<CommitTag>? tags, IReadOnlyList<string>? files)
		{
			Number = commit.Number;
			Author = author.Name;
			AuthorInfo = author.ToThinApiResponse();
			Description = commit.Description;

			if (tags != null)
			{
				Tags = new List<CommitTag>(tags);
			}

			if (files != null)
			{
				Files = new List<string>(files);
			}
		}
	}
}
