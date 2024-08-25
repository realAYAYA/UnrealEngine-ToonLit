// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Security.Claims;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Text.RegularExpressions;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Acls;
using EpicGames.Horde.Agents.Pools;
using EpicGames.Horde.Common;
using EpicGames.Horde.Issues;
using EpicGames.Horde.Jobs.Templates;
using EpicGames.Horde.Replicators;
using EpicGames.Horde.Streams;
using EpicGames.Horde.Telemetry;
using Horde.Server.Acls;
using Horde.Server.Configuration;
using Horde.Server.Issues;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Jobs.Templates;
using Horde.Server.Perforce;
using Horde.Server.Projects;
using Horde.Server.Replicators;
using Horde.Server.Server;
using Horde.Server.Utilities;
using HordeCommon;
using HordeCommon.Rpc.Tasks;

namespace Horde.Server.Streams
{
	/// <summary>
	/// Flags identifying content of a changelist
	/// </summary>
	[Flags]
	[Obsolete("Use tags instead")]
	public enum ChangeContentFlags
	{
		/// <summary>
		/// The change contains code
		/// </summary>
		ContainsCode = 1,

		/// <summary>
		/// The change contains content
		/// </summary>
		ContainsContent = 2,
	}

	/// <summary>
	/// How to replicate data for this stream
	/// </summary>
	public enum ContentReplicationMode
	{
		/// <summary>
		/// No content will be replicated for this stream
		/// </summary>
		None,

		/// <summary>
		/// Only replicate depot path and revision data for each file
		/// </summary>
		RevisionsOnly,

		/// <summary>
		/// Replicate full stream contents to storage
		/// </summary>
		Full,
	}

	/// <summary>
	/// Config for a stream
	/// </summary>
	[JsonSchema("https://unrealengine.com/horde/stream")]
	[JsonSchemaCatalog("Horde Stream", "Horde stream configuration file", new[] { "*.stream.json", "Streams/*.json" })]
	[ConfigIncludeRoot]
	[ConfigMacroScope]
	public class StreamConfig
	{
		/// <summary>
		/// Accessor for the project containing this stream
		/// </summary>
		[JsonIgnore]
		public ProjectConfig ProjectConfig { get; private set; } = null!;

		/// <summary>
		/// Identifier for the stream
		/// </summary>
		public StreamId Id { get; set; }

		/// <summary>
		/// Direct include path for the stream config. For backwards compatibility with old config files when including from a ProjectConfig object.
		/// </summary>
		[ConfigInclude, ConfigRelativePath]
		public string? Path { get; set; }

		/// <summary>
		/// Includes for other configuration files
		/// </summary>
		public List<ConfigInclude> Include { get; set; } = new List<ConfigInclude>();

		/// <summary>
		/// Macros within this stream
		/// </summary>
		public List<ConfigMacro> Macros { get; set; } = new List<ConfigMacro>();

		/// <summary>
		/// Revision identifier for this configuration object
		/// </summary>
		[JsonIgnore]
		public string Revision { get; set; } = String.Empty;

		/// <summary>
		/// Name of the stream
		/// </summary>
		public string Name { get; set; } = String.Empty;

		/// <summary>
		/// The perforce cluster containing the stream
		/// </summary>
		public string ClusterName { get; set; } = PerforceCluster.DefaultName;

		/// <summary>
		/// Order for this stream
		/// </summary>
		public int Order { get; set; } = 128;

		/// <summary>
		/// Default initial agent type for templates
		/// </summary>
		public string? InitialAgentType { get; set; }

		/// <summary>
		/// Notification channel for all jobs in this stream
		/// </summary>
		public string? NotificationChannel { get; set; }

		/// <summary>
		/// Notification channel filter for this template. Can be Success, Failure, or Warnings.
		/// </summary>
		public string? NotificationChannelFilter { get; set; }

		/// <summary>
		/// Channel to post issue triage notifications
		/// </summary>
		public string? TriageChannel { get; set; }

		/// <summary>
		/// Default settings for executing jobs
		/// </summary>
		public JobOptions JobOptions { get; set; } = new JobOptions();

		/// <summary>
		/// Telemetry store for Horde data for this stream
		/// </summary>
		public TelemetryStoreId TelemetryStoreId { get; set; }

		/// <summary>
		/// View for the AutoSDK paths to sync. If null, the whole thing will be synced.
		/// </summary>
		public List<string>? AutoSdkView { get; set; }

		/// <summary>
		/// Legacy name for the default preflight template
		/// </summary>
		[Obsolete("Use DefaultPreflight instead")]
		public string? DefaultPreflightTemplate
		{
			get => DefaultPreflight?.TemplateId?.ToString();
			set => DefaultPreflight = (value == null) ? null : new DefaultPreflightConfig { TemplateId = new TemplateId(value) };
		}

		/// <summary>
		/// Default template for running preflights
		/// </summary>
		public DefaultPreflightConfig? DefaultPreflight { get; set; }

		/// <summary>
		/// List of tags to apply to commits. Allows fast searching and classification of different commit types (eg. code vs content).
		/// </summary>
		public List<CommitTagConfig> CommitTags { get; set; } = new List<CommitTagConfig>();

		/// <summary>
		/// List of tabs to show for the new stream
		/// </summary>
		public List<TabConfig> Tabs { get; set; } = new List<TabConfig>();

		/// <summary>
		/// Global environment variables for all agents in this stream
		/// </summary>
		public Dictionary<string, string>? Environment { get; set; }

		/// <summary>
		/// Map of agent name to type
		/// </summary>
		public Dictionary<string, AgentConfig> AgentTypes { get; set; } = new Dictionary<string, AgentConfig>();

		/// <summary>
		/// Map of workspace name to type
		/// </summary>
		public Dictionary<string, WorkspaceConfig> WorkspaceTypes { get; set; } = new Dictionary<string, WorkspaceConfig>();

		/// <summary>
		/// List of templates to create
		/// </summary>
		public List<TemplateRefConfig> Templates { get; set; } = new List<TemplateRefConfig>();

		/// <summary>
		/// Custom permissions for this object
		/// </summary>
		public AclConfig Acl { get; set; } = new AclConfig();

		/// <summary>
		/// Pause stream builds until specified date
		/// </summary>
		public DateTime? PausedUntil { get; set; }

		/// <summary>
		/// Reason for pausing builds of the stream
		/// </summary>
		public string? PauseComment { get; set; }

		/// <summary>
		/// Configuration for workers to replicate commit data into Horde Storage.
		/// </summary>
		public List<ReplicatorConfig> Replicators { get; set; } = new List<ReplicatorConfig>();

		/// <summary>
		/// Workflows for dealing with new issues
		/// </summary>
		public List<WorkflowConfig> Workflows { get; set; } = new List<WorkflowConfig>();

		/// <summary>
		/// Tokens to create for each job step
		/// </summary>
		public List<TokenConfig> Tokens { get; set; } = new List<TokenConfig>();

		/// <inheritdoc cref="AclConfig.Authorize(AclAction, ClaimsPrincipal)"/>
		public bool Authorize(AclAction action, ClaimsPrincipal user)
			=> Acl.Authorize(action, user);

		/// <summary>
		/// Callback after reading this stream configuration
		/// </summary>
		/// <param name="id">The stream id</param>
		/// <param name="projectConfig">Owning project</param>
		public void PostLoad(StreamId id, ProjectConfig projectConfig)
		{
			Id = id;
			ProjectConfig = projectConfig;

			Acl.PostLoad(projectConfig.Acl, $"stream:{Id}");

			JobOptions.MergeDefaults(projectConfig.JobOptions);

			if (TelemetryStoreId.IsEmpty)
			{
				TelemetryStoreId = projectConfig.TelemetryStoreId;
			}

			foreach (TemplateRefConfig template in Templates)
			{
				template.PostLoad(this);
			}

			ConfigType.MergeDefaults(AgentTypes.Select(x => (x.Key, x.Value.Base, x.Value)));
			ConfigType.MergeDefaults(WorkspaceTypes.Select(x => (x.Key, x.Value.Base, x.Value)));
			ConfigType.MergeDefaults(Templates.Select(x => (x.Id, x.Base, x)));

			foreach (TemplateRefConfig template in Templates)
			{
				template.JobOptions.MergeDefaults(JobOptions);
			}

			foreach (TemplateRefConfig template in Templates)
			{
				foreach (ParameterData parameter in template.Parameters)
				{
					parameter.PostLoad();
				}
			}

			foreach (TemplateRefConfig template in Templates)
			{
				ScheduleConfig? schedule = template.Schedule;
				if (schedule != null)
				{
					foreach (CommitTag commitTag in schedule.Commits)
					{
						if (!TryGetCommitTag(commitTag, out _))
						{
							throw new InvalidOperationException($"Missing definition for commit tag '{commitTag}' referenced by {Id}:{template.Id}");
						}
					}
				}
			}

			if (Environment != null && Environment.Count > 0)
			{
				foreach (AgentConfig agentConfig in AgentTypes.Values)
				{
					agentConfig.Environment ??= new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
					foreach ((string key, string value) in Environment)
					{
						agentConfig.Environment.TryAdd(key, value);
					}
				}
			}
		}

		/// <summary>
		/// Enumerates all commit tags, including the default tags for code and content.
		/// </summary>
		/// <returns></returns>
		public IEnumerable<CommitTagConfig> GetAllCommitTags()
		{
			if (TryGetCommitTag(CommitTag.Code, out CommitTagConfig? codeConfig))
			{
				yield return codeConfig;
			}
			if (TryGetCommitTag(CommitTag.Content, out CommitTagConfig? contentConfig))
			{
				yield return contentConfig;
			}
			foreach (CommitTagConfig config in CommitTags)
			{
				if (config.Name != CommitTag.Code && config.Name != CommitTag.Content)
				{
					yield return config;
				}
			}
		}

		/// <summary>
		/// Gets the configuration for a commit tag
		/// </summary>
		/// <param name="tag">Tag to search for</param>
		/// <param name="config">Receives the tag configuration</param>
		/// <returns>True if a match was found</returns>
		public bool TryGetCommitTag(CommitTag tag, [NotNullWhen(true)] out CommitTagConfig? config)
		{
			CommitTagConfig? first = CommitTags.FirstOrDefault(x => x.Name == tag);
			if (first != null)
			{
				config = first;
				return true;
			}
			else if (tag == CommitTag.Code)
			{
				config = CommitTagConfig.CodeDefault;
				return true;
			}
			else if (tag == CommitTag.Content)
			{
				config = CommitTagConfig.ContentDefault;
				return true;
			}
			else
			{
				config = null;
				return false;
			}
		}

		/// <summary>
		/// Constructs a <see cref="FileFilter"/> from the rules in this configuration object
		/// </summary>
		/// <returns>Filter object</returns>
		public bool TryGetCommitTagFilter(CommitTag tag, [NotNullWhen(true)] out FileFilter? filter)
		{
			FileFilter newFilter = new FileFilter(FileFilterType.Exclude);
			if (TryGetCommitTagFilter(tag, newFilter, new HashSet<CommitTag>()))
			{
				filter = newFilter;
				return true;
			}
			else
			{
				filter = null;
				return false;
			}
		}

		/// <summary>
		/// Find the filter for a given tag, recursively
		/// </summary>
		bool TryGetCommitTagFilter(CommitTag tag, FileFilter filter, HashSet<CommitTag> visitedTags)
		{
			// Check we don't have a recursive definition for the tag
			if (!visitedTags.Add(tag))
			{
				return false;
			}

			// Get the tag configuration
			CommitTagConfig? config;
			if (!TryGetCommitTag(tag, out config))
			{
				return false;
			}

			// Add rules from the base tag
			if (!config.Base.IsEmpty() && !TryGetCommitTagFilter(config.Base, filter, visitedTags))
			{
				return false;
			}

			// Add rules from this tag
			filter.AddRules(config.Filter);
			return true;
		}

		/// <summary>
		/// Tries to find a replicator with the given id
		/// </summary>
		/// <param name="replicatorId"></param>
		/// <param name="replicatorConfig"></param>
		/// <returns></returns>
		public bool TryGetReplicator(StreamReplicatorId replicatorId, [NotNullWhen(true)] out ReplicatorConfig? replicatorConfig)
		{
			replicatorConfig = Replicators.FirstOrDefault(x => x.Id == replicatorId);
			return replicatorConfig != null;
		}

		/// <summary>
		/// Tries to find a template with the given id
		/// </summary>
		/// <param name="templateRefId"></param>
		/// <param name="templateConfig"></param>
		/// <returns></returns>
		public bool TryGetTemplate(TemplateId templateRefId, [NotNullWhen(true)] out TemplateRefConfig? templateConfig)
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

		/// <summary>
		/// The escaped name of this stream. Removes all non-identifier characters.
		/// </summary>
		/// <returns>Escaped name for the stream</returns>
		public string GetEscapedName()
		{
			return Regex.Replace(Name, @"[^a-zA-Z0-9_]", "+");
		}

		/// <summary>
		/// Gets the default identifier for workspaces created for this stream. Just includes an escaped depot name.
		/// </summary>
		/// <returns>The default workspace identifier</returns>
		public string GetDefaultWorkspaceIdentifier()
		{
			return Regex.Replace(GetEscapedName(), @"^(\+\+[^+]*).*$", "$1");
		}
	}

	/// <summary>
	/// Style for rendering a tab
	/// </summary>
	public enum TabStyle
	{
		/// <summary>
		/// Regular job list
		/// </summary>
		Normal,

		/// <summary>
		/// Omit job names, show condensed view
		/// </summary>
		Compact,
	}

	/// <summary>
	/// Information about a page to display in the dashboard for a stream
	/// </summary>
	public class TabConfig
	{
		/// <summary>
		/// Title of this page
		/// </summary>
		[Required]
		public string Title { get; set; } = null!;

		/// <summary>
		/// Type of this tab
		/// </summary>
		public string Type { get; set; } = "Jobs";

		/// <summary>
		/// Presentation style for this page
		/// </summary>
		public TabStyle Style { get; set; }

		/// <summary>
		/// Whether to show job names on this page
		/// </summary>
		public bool ShowNames
		{
			get => _showNames ?? (Style != TabStyle.Compact);
			set => _showNames = value;
		}
		bool? _showNames;

		/// <summary>
		/// Whether to show all user preflights 
		/// </summary>
		public bool? ShowPreflights { get; set; }

		/// <summary>
		/// Names of jobs to include on this page. If there is only one name specified, the name column does not need to be displayed.
		/// </summary>
		public List<string>? JobNames { get; set; }

		/// <summary>
		/// List of job template names to show on this page.
		/// </summary>
		public List<TemplateId>? Templates { get; set; }

		/// <summary>
		/// Columns to display for different types of aggregates
		/// </summary>
		public List<TabColumnConfig>? Columns { get; set; }
	}

	/// <summary>
	/// Type of a column in a jobs tab
	/// </summary>
	public enum TabColumnType
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
	public class TabColumnConfig
	{
		/// <summary>
		/// The type of column
		/// </summary>
		public TabColumnType Type { get; set; } = TabColumnType.Labels;

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
	}

	/// <summary>
	/// Mapping from a BuildGraph agent type to a set of machines on the farm
	/// </summary>
	public class AgentConfig
	{
		/// <summary>
		/// Base agent config to inherit settings from
		/// </summary>
		public string? Base { get; set; }

		/// <summary>
		/// Pool of agents to use for this agent type
		/// </summary>
		public PoolId Pool { get; set; }

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
		/// Tokens to allocate for this agent type
		/// </summary>
		[ConfigMergeStrategy(ConfigMergeStrategy.Append)]
		public List<TokenConfig>? Tokens { get; set; }

		/// <summary>
		/// Creates an API response object from this stream
		/// </summary>
		/// <returns>The response object</returns>
		public HordeCommon.Rpc.GetAgentTypeResponse ToRpcResponse()
		{
			HordeCommon.Rpc.GetAgentTypeResponse response = new HordeCommon.Rpc.GetAgentTypeResponse();
			if (TempStorageDir != null)
			{
				response.TempStorageDir = TempStorageDir;
			}
			if (Environment != null)
			{
				response.Environment.Add(Environment);
			}
			return response;
		}
	}

	/// <summary>
	/// Information about a workspace type
	/// </summary>
	public class WorkspaceConfig
	{
		/// <summary>
		/// Base workspace to derive from
		/// </summary>
		public string? Base { get; set; }

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
		[ConfigMergeStrategy(ConfigMergeStrategy.Append)]
		public List<string>? View { get; set; }

		/// <summary>
		/// Whether to use an incrementally synced workspace
		/// </summary>
		public bool? Incremental { get; set; }

		/// <summary>
		/// Whether to use the AutoSDK
		/// </summary>
		public bool? UseAutoSdk { get; set; }

		/// <summary>
		/// View for the AutoSDK paths to sync. If null, the whole thing will be synced.
		/// </summary>
		[ConfigMergeStrategy(ConfigMergeStrategy.Append)]
		public List<string>? AutoSdkView { get; set; }

		/// <summary>
		/// Method to use when syncing/materializing data from Perforce
		/// </summary>
		public string? Method { get; set; } = null;
	}

	/// <summary>
	/// Query selecting the base changelist to use
	/// </summary>
	public class ChangeQueryConfig
	{
		/// <summary>
		/// Name of this query, for display on the dashboard.
		/// </summary>
		public string? Name { get; set; }

		/// <summary>
		/// Condition to evaluate before deciding to use this query. May query tags in a preflight.
		/// </summary>
		public Condition? Condition { get; set; }

		/// <summary>
		/// The template id to query
		/// </summary>
		public TemplateId? TemplateId { get; set; }

		/// <summary>
		/// The target to query
		/// </summary>
		public string? Target { get; set; }

		/// <summary>
		/// Whether to match a job that produced warnings
		/// </summary>
		public List<JobStepOutcome>? Outcomes { get; set; }

		/// <summary>
		/// Finds the last commit with this tag
		/// </summary>
		public CommitTag? CommitTag { get; set; }
	}

	/// <summary>
	/// Specifies defaults for running a preflight
	/// </summary>
	public class DefaultPreflightConfig
	{
		/// <summary>
		/// The template id to query
		/// </summary>
		public TemplateId? TemplateId { get; set; }

		/// <summary>
		/// Query for the change to use
		/// </summary>
		[Obsolete("Set on template instead")]
		public ChangeQueryConfig? Change { get; set; }
	}

	/// <summary>
	/// Trigger for another template
	/// </summary>
	public class ChainedJobTemplateConfig
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
		public TemplateId TemplateId { get; set; }

		/// <summary>
		/// Whether to use the default change for the template rather than the change for the parent job.
		/// </summary>
		public bool UseDefaultChangeForTemplate { get; set; }
	}

	/// <summary>
	/// Parameters to create a template within a stream
	/// </summary>
	[DebuggerDisplay("{Id}")]
	public class TemplateRefConfig : TemplateConfig
	{
		/// <summary>
		/// The owning stream config
		/// </summary>
		[JsonIgnore]
		public StreamConfig StreamConfig { get; private set; } = null!;

		/// <summary>
		/// Optional identifier for this ref. If not specified, an id will be generated from the name.
		/// </summary>
		public TemplateId Id { get; set; }

		/// <summary>
		/// Base template id to copy from
		/// </summary>
		public TemplateId Base { get; set; }

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
		[ConfigMergeStrategy(ConfigMergeStrategy.Recursive)]
		public ScheduleConfig? Schedule { get; set; }

		/// <summary>
		/// List of chained job triggers
		/// </summary>
		[ConfigMergeStrategy(ConfigMergeStrategy.Append)]
		public List<ChainedJobTemplateConfig>? ChainedJobs { get; set; }

		/// <summary>
		/// The ACL for this template
		/// </summary>
		[ConfigMergeStrategy(ConfigMergeStrategy.Recursive)]
		public AclConfig Acl { get; set; } = new AclConfig();

		/// <inheritdoc cref="AclConfig.Authorize(AclAction, ClaimsPrincipal)"/>
		public bool Authorize(AclAction action, ClaimsPrincipal user)
			=> Acl.Authorize(action, user);

		/// <summary>
		/// Callback after the config is loaded
		/// </summary>
		/// <param name="streamConfig"></param>
		public void PostLoad(StreamConfig streamConfig)
		{
			StreamConfig = streamConfig;

			if (Id.IsEmpty)
			{
				Id = new TemplateId(StringId.Sanitize(Name));
			}

			Acl.PostLoad(streamConfig.Acl, $"template:{Id}");
		}
	}

	/// <summary>
	/// Parameters to create a new schedule
	/// </summary>
	public class SchedulePatternConfig
	{
		/// <summary>
		/// Days of the week to run this schedule on. If null, the schedule will run every day.
		/// </summary>
		public List<DayOfWeek>? DaysOfWeek { get; set; }

		/// <summary>
		/// Time during the day for the first schedule to trigger. Measured in minutes from midnight.
		/// </summary>
		public ScheduleTimeOfDay MinTime { get; set; } = new ScheduleTimeOfDay(0);

		/// <summary>
		/// Time during the day for the last schedule to trigger. Measured in minutes from midnight.
		/// </summary>
		public ScheduleTimeOfDay? MaxTime { get; set; }

		/// <summary>
		/// Interval between each schedule triggering
		/// </summary>
		public ScheduleInterval? Interval { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		public SchedulePatternConfig()
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="daysOfWeek">Which days of the week the schedule should run</param>
		/// <param name="minTime">Time during the day for the first schedule to trigger. Measured in minutes from midnight.</param>
		/// <param name="maxTime">Time during the day for the last schedule to trigger. Measured in minutes from midnight.</param>
		/// <param name="interval">Interval between each schedule triggering</param>
		public SchedulePatternConfig(List<DayOfWeek>? daysOfWeek, int minTime, int? maxTime, int? interval)
		{
			DaysOfWeek = daysOfWeek;
			MinTime = new ScheduleTimeOfDay(minTime);
			MaxTime = (maxTime != null) ? new ScheduleTimeOfDay(maxTime.Value) : null;
			Interval = (interval != null) ? new ScheduleInterval(interval.Value) : null;
		}

		/// <summary>
		/// Calculates the trigger index based on the given time in minutes
		/// </summary>
		/// <param name="lastTimeUtc">Time for the last trigger</param>
		/// <param name="timeZone">The timezone for running the schedule</param>
		/// <returns>Index of the trigger</returns>
		public DateTime GetNextTriggerTimeUtc(DateTime lastTimeUtc, TimeZoneInfo timeZone)
		{
			// Convert last time into the correct timezone for running the scheule
			DateTimeOffset lastTime = TimeZoneInfo.ConvertTime((DateTimeOffset)lastTimeUtc, timeZone);

			// Get the base time (ie. the start of this day) for anchoring the schedule
			DateTimeOffset baseTime = new DateTimeOffset(lastTime.Year, lastTime.Month, lastTime.Day, 0, 0, 0, lastTime.Offset);
			for (; ; )
			{
				if (DaysOfWeek == null || DaysOfWeek.Contains(baseTime.DayOfWeek))
				{
					// Get the last time in minutes from the start of this day
					int lastTimeMinutes = (int)(lastTime - baseTime).TotalMinutes;

					// Get the time of the first trigger of this day. If the last time is less than this, this is the next trigger.
					if (lastTimeMinutes < MinTime.Minutes)
					{
						return baseTime.AddMinutes(MinTime.Minutes).UtcDateTime;
					}

					// Otherwise, get the time for the last trigger in the day.
					if (Interval != null && Interval.Minutes > 0)
					{
						int actualMaxTime = MaxTime?.Minutes ?? ((24 * 60) - 1);
						if (lastTimeMinutes < actualMaxTime)
						{
							int lastIndex = (lastTimeMinutes - MinTime.Minutes) / Interval.Minutes;
							int nextIndex = lastIndex + 1;

							int nextTimeMinutes = MinTime.Minutes + (nextIndex * Interval.Minutes);
							if (nextTimeMinutes <= actualMaxTime)
							{
								return baseTime.AddMinutes(nextTimeMinutes).UtcDateTime;
							}
						}
					}
				}
				baseTime = baseTime.AddDays(1.0);
			}
		}
	}

	/// <summary>
	/// Time of day value for a schedule
	/// </summary>
	[JsonSchemaString]
	[JsonConverter(typeof(ScheduleTimeOfDayJsonConverter))]
	public record class ScheduleTimeOfDay(int Minutes)
	{
		/// <summary>
		/// Parse a string as a time of day
		/// </summary>
		[return: NotNullIfNotNull("text")]
		public static ScheduleTimeOfDay? Parse(string? text)
			=> (text != null) ? new ScheduleTimeOfDay((int)TimeOfDayJsonConverter.Parse(text).TotalMinutes) : null;
	}

	class ScheduleTimeOfDayJsonConverter : JsonConverter<ScheduleTimeOfDay>
	{
		/// <inheritdoc/>
		public override ScheduleTimeOfDay? Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
		{
			if (reader.TokenType == JsonTokenType.Number)
			{
				return new ScheduleTimeOfDay(reader.GetInt32());
			}
			else
			{
				return ScheduleTimeOfDay.Parse(reader.GetString());
			}
		}

		/// <inheritdoc/>
		public override void Write(Utf8JsonWriter writer, ScheduleTimeOfDay value, JsonSerializerOptions options)
		{
			writer.WriteNumberValue(value.Minutes);
		}
	}

	/// <summary>
	/// Time of day value for a schedule
	/// </summary>
	[JsonSchemaString]
	[JsonConverter(typeof(ScheduleIntervalJsonConverter))]
	public record class ScheduleInterval(int Minutes)
	{
		/// <summary>
		/// Parse a string as a time of day
		/// </summary>
		[return: NotNullIfNotNull("text")]
		public static ScheduleInterval? Parse(string? text)
			=> (text != null) ? new ScheduleInterval((int)IntervalJsonConverter.Parse(text).TotalMinutes) : null;
	}

	class ScheduleIntervalJsonConverter : JsonConverter<ScheduleInterval>
	{
		/// <inheritdoc/>
		public override ScheduleInterval? Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
		{
			if (reader.TokenType == JsonTokenType.Number)
			{
				return new ScheduleInterval(reader.GetInt32());
			}
			else
			{
				return ScheduleInterval.Parse(reader.GetString());
			}
		}

		/// <inheritdoc/>
		public override void Write(Utf8JsonWriter writer, ScheduleInterval value, JsonSerializerOptions options)
		{
			writer.WriteNumberValue(value.Minutes);
		}
	}

	/// <summary>
	/// Gate allowing a schedule to trigger.
	/// </summary>
	public class ScheduleGateConfig
	{
		/// <summary>
		/// The template containing the dependency
		/// </summary>
		[Required]
		public TemplateId TemplateId { get; set; }

		/// <summary>
		/// Target to wait for
		/// </summary>
		[Required]
		public string Target { get; set; } = String.Empty;
	}

	/// <summary>
	/// Parameters to create a new schedule
	/// </summary>
	public class ScheduleConfig
	{
		/// <summary>
		/// Roles to impersonate for this schedule
		/// </summary>
		public List<AclClaimConfig>? Claims { get; set; }

		/// <summary>
		/// Whether the schedule should be enabled
		/// </summary>
		public bool Enabled { get; set; } = true;

		/// <summary>
		/// Maximum number of builds that can be active at once
		/// </summary>
		public int MaxActive { get; set; }

		/// <summary>
		/// Maximum number of changes the schedule can fall behind head revision. If greater than zero, builds will be triggered for every submitted changelist until the backlog is this size.
		/// </summary>
		public int MaxChanges { get; set; }

		/// <summary>
		/// Whether the build requires a change to be submitted
		/// </summary>
		public bool RequireSubmittedChange { get; set; } = true;

		/// <summary>
		/// Gate allowing the schedule to trigger
		/// </summary>
		public ScheduleGateConfig? Gate { get; set; }

		/// <summary>
		/// Commit tags for this schedule
		/// </summary>
		public List<CommitTag> Commits { get; set; } = new List<CommitTag>();

		/// <summary>
		/// The types of changes to run for
		/// </summary>
		[Obsolete("Use Commits instead")]
		public List<ChangeContentFlags>? Filter { get; set; }

		/// <summary>
		/// Merges the list of commit tags with the legacy filter field
		/// </summary>
#pragma warning disable CS0618 // Type or member is obsolete
		internal List<CommitTag> GetAllCommitTags()
		{
			List<CommitTag> commits = Commits;
			if (Filter != null && Filter.Count > 0)
			{
				commits = new List<CommitTag>(commits);
				if (Filter.Contains(ChangeContentFlags.ContainsCode))
				{
					commits.Add(CommitTag.Code);
				}
				if (Filter.Contains(ChangeContentFlags.ContainsContent))
				{
					commits.Add(CommitTag.Content);
				}
			}
			return commits;
		}
#pragma warning restore CS0618 // Type or member is obsolete

		/// <summary>
		/// Files that should cause the job to trigger
		/// </summary>
		public List<string>? Files { get; set; }

		/// <summary>
		/// Parameters for the template
		/// </summary>
		public Dictionary<string, string> TemplateParameters { get; set; } = new Dictionary<string, string>();

		/// <summary>
		/// New patterns for the schedule
		/// </summary>
		public List<SchedulePatternConfig> Patterns { get; set; } = new List<SchedulePatternConfig>();

		/// <summary>
		/// Get the next time that the schedule will trigger
		/// </summary>
		/// <param name="lastTimeUtc">Last time at which the schedule triggered</param>
		/// <param name="timeZone">Timezone to evaluate the trigger</param>
		/// <returns>Next time at which the schedule will trigger</returns>
		public DateTime? GetNextTriggerTimeUtc(DateTime lastTimeUtc, TimeZoneInfo timeZone)
		{
			DateTime? nextTriggerTimeUtc = null;
			foreach (SchedulePatternConfig pattern in Patterns)
			{
				DateTime patternTriggerTime = pattern.GetNextTriggerTimeUtc(lastTimeUtc, timeZone);
				if (nextTriggerTimeUtc == null || patternTriggerTime < nextTriggerTimeUtc)
				{
					nextTriggerTimeUtc = patternTriggerTime;
				}
			}
			return nextTriggerTimeUtc;
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

	/// <summary>
	/// Configuration for custom commit filters
	/// </summary>
	public class CommitTagConfig
	{
		/// <summary>
		/// Name of the tag
		/// </summary>
		[Required]
		public CommitTag Name { get; set; }

		/// <summary>
		/// Base tag to copy settings from
		/// </summary>
		public CommitTag Base { get; set; }

		/// <summary>
		/// List of files to be included in this filter
		/// </summary>
		public List<string> Filter { get; set; } = new List<string>();

		/// <summary>
		/// Default config for code filters
		/// </summary>
		public static CommitTagConfig CodeDefault { get; } = new CommitTagConfig { Name = CommitTag.Code, Filter = CommitTag.CodeFilter.ToList() };

		/// <summary>
		/// Default config for content filters
		/// </summary>
		public static CommitTagConfig ContentDefault { get; } = new CommitTagConfig { Name = CommitTag.Content, Filter = CommitTag.ContentFilter.ToList() };
	}
}
