// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using EpicGames.Core;
using Horde.Build.Acls;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Jobs.Templates;
using Horde.Build.Server;
using Horde.Build.Jobs.Schedules;
using Horde.Build.Users;
using Horde.Build.Utilities;
using HordeCommon;
using Horde.Build.Issues;

namespace Horde.Build.Streams
{
	using TemplateRefId = StringId<TemplateRef>;
	using WorkflowId = StringId<WorkflowConfig>;

	/// <summary>
	/// Query selecting the base changelist to use
	/// </summary>
	public class ChangeQueryRequest
	{
		/// <summary>
		/// The template id to query
		/// </summary>
		public string? TemplateId { get; set; }

		/// <summary>
		/// The target to query
		/// </summary>
		public string? Target { get; set; }

		/// <summary>
		/// Whether to match a job that produced warnings
		/// </summary>
		public List<JobStepOutcome>? Outcomes { get; set; }

		/// <summary>
		/// Convert to a model object
		/// </summary>
		/// <returns></returns>
		public ChangeQuery ToModel()
		{
			ChangeQuery query = new ChangeQuery();
			if (TemplateId != null)
			{
				query.TemplateRefId = new StringId<TemplateRef>(TemplateId);
			}
			query.Target = Target;
			query.Outcomes = Outcomes;
			return query;
		}
	}

	/// <summary>
	/// Specifies defaults for running a preflight
	/// </summary>
	public class DefaultPreflightRequest
	{
		/// <summary>
		/// The template id to query
		/// </summary>
		public string? TemplateId { get; set; }

		/// <summary>
		/// The last successful job type to use for the base changelist
		/// </summary>
		[Obsolete("Use Change.TemplateId instead")]
		public string? ChangeTemplateId { get; set; }

		/// <summary>
		/// Query for the change to use
		/// </summary>
		public ChangeQueryRequest? Change { get; set; }

		/// <summary>
		/// Convert to a model object
		/// </summary>
		/// <returns></returns>
		public DefaultPreflight ToModel()
		{
#pragma warning disable CS0618 // Type or member is obsolete
			if (ChangeTemplateId != null)
			{
				Change ??= new ChangeQueryRequest();
				Change.TemplateId = ChangeTemplateId;
			}
			return new DefaultPreflight((TemplateId != null) ? (TemplateRefId?)new TemplateRefId(TemplateId) : null, Change?.ToModel());
#pragma warning restore CS0618 // Type or member is obsolete
		}
	}

	/// <summary>
	/// Mapping from a BuildGraph agent type to a set of machines on the farm
	/// </summary>
	public class CreateAgentTypeRequest
	{
		/// <summary>
		/// Pool of agents to use for this agent type
		/// </summary>
		[Required]
		public string Pool { get; set; } = null!;

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
	}

	/// <summary>
	/// Information about a workspace type
	/// </summary>
	public class CreateWorkspaceTypeRequest
	{
		/// <summary>
		/// Name of the Perforce server cluster to use
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
		/// Password to use to log into the workspace
		/// </summary>
		public string? Password { get; set; }

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
		public bool UseAutoSdk { get; set; } = true;
	}

	/// <summary>
	/// Trigger for another template
	/// </summary>
	public class CreateChainedJobTemplateRequest
	{
		/// <summary>
		/// Name of the target that needs to complete before starting the other template
		/// </summary>
		[Required]
		public string Trigger { get; set; } = String.Empty;

		/// <summary>
		/// Id of the template to trigger
		/// </summary>
		[Required]
		public string TemplateId { get; set; } = String.Empty;
	}

	/// <summary>
	/// Parameters to create a template within a stream
	/// </summary>
	public class TemplateRefConfig : TemplateConfig
	{
		TemplateRefId _id;

		/// <summary>
		/// Optional identifier for this ref. If not specified, an id will be generated from the name.
		/// </summary>
		public TemplateRefId Id
		{
			get => _id.IsEmpty ? TemplateRefId.Sanitize(Name) : _id;
			set => _id = value;
		}

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
		/// Notification channel filter for this template. Can be Success|Failure|Warnings
		/// </summary>
		public string? NotificationChannelFilter { get; set; }

		/// <summary>
		/// Triage channel for this template. Overrides the stream channel if set.
		/// </summary>
		public string? TriageChannel { get; set; }

		/// <summary>
		/// Workflow to user for this stream
		/// </summary>
		public WorkflowId? WorkflowId
		{
			get => Annotations.WorkflowId;
			set => Annotations.WorkflowId = value;
		}

		/// <summary>
		/// Default annotations to apply to nodes in this template
		/// </summary>
		public NodeAnnotations Annotations { get; set; } = new NodeAnnotations();

		/// <summary>
		/// Schedule to execute this template
		/// </summary>
		public CreateScheduleRequest? Schedule { get; set; }

		/// <summary>
		/// List of chained job triggers
		/// </summary>
		public List<CreateChainedJobTemplateRequest>? ChainedJobs { get; set; }

		/// <summary>
		/// The ACL for this template
		/// </summary>
		public UpdateAclRequest? Acl { get; set; }
	}

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
		/// <param name="bIncremental">Whether to use an incrementally synced workspace</param>
		/// <param name="bUseAutoSdk">Whether to use the AutoSDK</param>
		public GetWorkspaceTypeResponse(string? cluster, string? serverAndPort, string? userName, string? identifier, string? stream, List<string>? view, bool bIncremental, bool bUseAutoSdk)
		{
			Cluster = cluster;
			ServerAndPort = serverAndPort;
			UserName = userName;
			Identifier = identifier;
			Stream = stream;
			View = view;
			Incremental = bIncremental;
			UseAutoSdk = bUseAutoSdk;
		}
	}

	/// <summary>
	/// Trigger for another template
	/// </summary>
	public class GetChainedJobTemplateResponse
	{
		/// <summary>
		/// Name of the target that needs to complete before starting the other template
		/// </summary>
		public string Trigger { get; set; }

		/// <summary>
		/// Id of the template to trigger
		/// </summary>
		public string TemplateId { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="trigger">The trigger definition</param>
		public GetChainedJobTemplateResponse(ChainedJobTemplate trigger)
		{
			Trigger = trigger.Trigger;
			TemplateId = trigger.TemplateRefId.ToString();
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
		public GetTemplateStepStateResponse(TemplateStepState state, GetThinUserInfoResponse? pausedByUserInfo)
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
		/// Notification channel filter for this template. Can be Success|Failure|Warnings
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
		public List<GetChainedJobTemplateResponse>? ChainedJobs { get; set; }

		/// <summary>
		/// List of step states
		/// </summary>
		public List<GetTemplateStepStateResponse>? StepStates { get; set; }

		/// <summary>
		/// ACL for this template
		/// </summary>
		public GetAclResponse? Acl { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="id">The template ref id</param>
		/// <param name="templateRef">The template ref</param>
		/// <param name="template">The template instance</param>
		/// <param name="stepStates">The template step states</param>
		/// <param name="bIncludeAcl">Whether to include the ACL in the response</param>
		public GetTemplateRefResponse(TemplateRefId id, TemplateRef templateRef, ITemplate template, List<GetTemplateStepStateResponse>? stepStates, bool bIncludeAcl)
			: base(template)
		{
			Id = id.ToString();
			Hash = templateRef.Hash.ToString();
			ShowUgsBadges = templateRef.ShowUgsBadges;
			ShowUgsAlerts = templateRef.ShowUgsAlerts;
			NotificationChannel = templateRef.NotificationChannel;
			NotificationChannelFilter = templateRef.NotificationChannelFilter;
			Schedule = (templateRef.Schedule != null) ? new GetScheduleResponse(templateRef.Schedule) : null;
			ChainedJobs = (templateRef.ChainedJobs != null && templateRef.ChainedJobs.Count > 0) ? templateRef.ChainedJobs.ConvertAll(x => new GetChainedJobTemplateResponse(x)) : null;
			StepStates = stepStates;
			Acl = (bIncludeAcl && templateRef.Acl != null)? new GetAclResponse(templateRef.Acl) : null;
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
		/// Default template to use for preflights
		/// </summary>
		public DefaultPreflightRequest? DefaultPreflight { get; set; }

		/// <summary>
		/// List of tabs to display for this stream
		/// </summary>
		public List<GetStreamTabResponse> Tabs { get; set; }

		/// <summary>
		/// Map of agent name to type
		/// </summary>
		public Dictionary<string, GetAgentTypeResponse> AgentTypes { get; set; }

		/// <summary>
		/// Map of workspace name to type
		/// </summary>
		public Dictionary<string, GetWorkspaceTypeResponse>? WorkspaceTypes { get; set; }

		/// <summary>
		/// Templates for jobs in this stream
		/// </summary>
		public List<GetTemplateRefResponse> Templates { get; set; }

		/// <summary>
		/// Custom permissions for this object
		/// </summary>
		public GetAclResponse? Acl { get; set; }
		
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
		/// <param name="id">Unique id of the stream</param>
		/// <param name="projectId">Unique id of the project containing the stream</param>
		/// <param name="name">Name of the stream</param>
		/// <param name="configRevision">The config file path on the server</param>
		/// <param name="order">Order to display this stream</param>
		/// <param name="notificationChannel"></param>
		/// <param name="notificationChannelFilter"></param>
		/// <param name="triageChannel"></param>
		/// <param name="defaultPreflight">The default template to use for preflights</param>
		/// <param name="tabs">List of tabs to display for this stream</param>
		/// <param name="agentTypes">Map of agent type name to description</param>
		/// <param name="workspaceTypes">Map of workspace name to description</param>
		/// <param name="templates">Templates for this stream</param>
		/// <param name="acl">Permissions for this object</param>
		/// <param name="pausedUntil">Stream paused for new builds until this date</param>
		/// <param name="pauseComment">Reason for stream being paused</param>
		/// <param name="workflows">Workflows for this stream</param>
		public GetStreamResponse(string id, string projectId, string name, string configRevision, int order, string? notificationChannel, string? notificationChannelFilter, string? triageChannel, DefaultPreflightRequest? defaultPreflight, List<GetStreamTabResponse> tabs, Dictionary<string, GetAgentTypeResponse> agentTypes, Dictionary<string, GetWorkspaceTypeResponse>? workspaceTypes, List<GetTemplateRefResponse> templates, GetAclResponse? acl, DateTime? pausedUntil, string? pauseComment, List<WorkflowConfig> workflows)
		{
			Id = id;
			ProjectId = projectId;
			Name = name;
			ConfigRevision = configRevision;
			Order = order;
			NotificationChannel = notificationChannel;
			NotificationChannelFilter = notificationChannelFilter;
			TriageChannel = triageChannel;
			DefaultPreflightTemplate = defaultPreflight?.TemplateId;
			DefaultPreflight = defaultPreflight;
			Tabs = tabs;
			AgentTypes = agentTypes;
			WorkspaceTypes = workspaceTypes;
			Templates = templates;
			Acl = acl;
			PausedUntil = pausedUntil;
			PauseComment = pauseComment;
			Workflows = workflows;
		}
	}

	/// <summary>
	/// Information about a page to display in the dashboard for a stream
	/// </summary>
	[JsonKnownTypes(typeof(CreateJobsTabRequest))]
	public abstract class CreateStreamTabRequest
	{
		/// <summary>
		/// Title of this page
		/// </summary>
		[Required]
		public string Title { get; set; } = null!;
	}

	/// <summary>
	/// Type of a column in a jobs tab
	/// </summary>
	public enum JobsTabColumnType
	{
		/// <summary>
		/// Contains labels
		/// </summary>
		Labels,

		/// <summary>
		/// Contains parameters
		/// </summary>
		Parameter
	}

	/// <summary>
	/// Describes a column to display on the jobs page
	/// </summary>
	public class CreateJobsTabColumnRequest
	{
		/// <summary>
		/// The type of column
		/// </summary>
		public JobsTabColumnType Type { get; set; } = JobsTabColumnType.Labels;

		/// <summary>
		/// Heading for this column
		/// </summary>
		[Required]
		public string Heading { get; set; } = null!;

		/// <summary>
		/// Category of aggregates to display in this column. If null, includes any aggregate not matched by another column.
		/// </summary>
		public string? Category { get; set; }

		/// <summary>
		/// Parameter to show in this column
		/// </summary>
		public string? Parameter { get; set; }

		/// <summary>
		/// Relative width of this column.
		/// </summary>
		public int? RelativeWidth { get; set; }

		/// <summary>
		/// Construct a JobsTabColumn object from this request
		/// </summary>
		/// <returns>Column object</returns>
		public JobsTabColumn ToModel()
		{
			switch (Type)
			{
				case JobsTabColumnType.Labels:
					return new JobsTabLabelColumn(Heading, Category, RelativeWidth);
				case JobsTabColumnType.Parameter:
					return new JobsTabParameterColumn(Heading, Parameter ?? "Undefined", RelativeWidth);
				default:
					return new JobsTabLabelColumn(Heading, "Undefined", RelativeWidth);
			}
		}
	}

	/// <summary>
	/// Describes a job page
	/// </summary>
	[JsonDiscriminator("Jobs")]
	public class CreateJobsTabRequest : CreateStreamTabRequest
	{
		/// <summary>
		/// Whether to show job names on this page
		/// </summary>
		public bool ShowNames { get; set; }

		/// <summary>
		/// Names of jobs to include on this page. If there is only one name specified, the name column does not need to be displayed.
		/// </summary>
		public List<string>? JobNames { get; set; }

		/// <summary>
		/// List of job template names to show on this page.
		/// </summary>
		public List<TemplateRefId>? Templates { get; set; }

		/// <summary>
		/// Columns to display for different types of aggregates
		/// </summary>
		public List<CreateJobsTabColumnRequest>? Columns { get; set; }
	}

	/// <summary>
	/// Information about a page to display in the dashboard for a stream
	/// </summary>
	[JsonKnownTypes(typeof(GetJobsTabResponse))]
	public abstract class GetStreamTabResponse
	{
		/// <summary>
		/// Title of this page
		/// </summary>
		public string Title { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="title">Title of this page</param>
		protected GetStreamTabResponse(string title)//, StreamPageType Type)
		{
			Title = title;
		}
	}

	/// <summary>
	/// Describes a column to display on the jobs page
	/// </summary>
	[JsonKnownTypes(typeof(GetJobsTabLabelColumnResponse), typeof(GetJobsTabParameterColumnResponse))]
	public abstract class GetJobsTabColumnResponse
	{
		/// <summary>
		/// Heading for this column
		/// </summary>
		public string Heading { get; set; } = null!;

		/// <summary>
		/// Relative width of this column.
		/// </summary>
		public int? RelativeWidth { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="heading">Heading for this column</param>
		/// <param name="relativeWidth">Relative width of this column</param>
		protected GetJobsTabColumnResponse(string heading, int? relativeWidth)
		{
			Heading = heading;
			RelativeWidth = relativeWidth;
		}
	}

	/// <summary>
	/// Describes a label column to display on the jobs page
	/// </summary>
	[JsonDiscriminator(nameof(JobsTabColumnType.Labels))]
	public class GetJobsTabLabelColumnResponse : GetJobsTabColumnResponse
	{
		/// <summary>
		/// Category of aggregates to display in this column. If null, includes any aggregate not matched by another column.
		/// </summary>
		public string? Category { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="heading">Heading for this column</param>
		/// <param name="category">Category of aggregates to display in this column. If null, includes any aggregate not matched by another column.</param>
		/// <param name="relativeWidth">Relative width of this column</param>
		public GetJobsTabLabelColumnResponse(string heading, string? category, int? relativeWidth)
			: base(heading, relativeWidth)
		{
			Category = category;
		}
	}

	/// <summary>
	/// Describes a parameter column to display on the jobs page
	/// </summary>
	[JsonDiscriminator(nameof(JobsTabColumnType.Parameter))]
	public class GetJobsTabParameterColumnResponse : GetJobsTabColumnResponse
	{
		/// <summary>
		/// Parameter to show in this column
		/// </summary>
		public string? Parameter { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="heading">Heading for this column</param>
		/// <param name="parameter">Name of the parameter to display</param>
		/// <param name="relativeWidth">Relative width of this column</param>
		public GetJobsTabParameterColumnResponse(string heading, string? parameter, int? relativeWidth)
			: base(heading, relativeWidth)
		{
			Parameter = parameter;
		}
	}

	/// <summary>
	/// Describes a job page
	/// </summary>
	[JsonDiscriminator("Jobs")]
	public class GetJobsTabResponse : GetStreamTabResponse
	{
		/// <summary>
		/// Whether to show names on the page
		/// </summary>
		public bool ShowNames { get; set; }

		/// <summary>
		/// List of templates to show on the page
		/// </summary>
		public List<TemplateRefId>? Templates { get; set; }

		/// <summary>
		/// Names of jobs to include on this page. If there is only one name specified, the name column does not need to be displayed.
		/// </summary>
		public List<string>? JobNames { get; set; }

		/// <summary>
		/// Columns to display for different types of aggregates
		/// </summary>
		public List<GetJobsTabColumnResponse>? Columns { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="title">Title for this page</param>
		/// <param name="bShowNames">Whether to show names on the page</param>
		/// <param name="templates">Templates to include on this page</param>
		/// <param name="jobNames">List of job names to include on this page</param>
		/// <param name="columns">List of columns to display</param>
		public GetJobsTabResponse(string title, bool bShowNames, List<TemplateRefId>? templates, List<string>? jobNames, List<GetJobsTabColumnResponse>? columns)
			: base(title)
		{
			ShowNames = bShowNames;
			Templates = templates;
			JobNames = jobNames;
			Columns = columns;
		}
	}
}
