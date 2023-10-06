// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Security.Claims;
using System.Text.Json.Serialization;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Nodes;
using EpicGames.Perforce;
using Horde.Server.Acls;
using Horde.Server.Agents;
using Horde.Server.Agents.Fleet;
using Horde.Server.Agents.Pools;
using Horde.Server.Agents.Sessions;
using Horde.Server.Agents.Software;
using Horde.Server.Jobs;
using Horde.Server.Logs;
using Horde.Server.Projects;
using Horde.Server.Server;
using Horde.Server.Storage.Backends;
using Horde.Server.Streams;
using Horde.Server.Telemetry;
using Horde.Server.Tools;
using Horde.Server.Utilities;
using Serilog.Events;

namespace Horde.Server
{
	/// <summary>
	/// Types of storage backend to use
	/// </summary>
	public enum StorageBackendType
	{
		/// <summary>
		/// Local filesystem
		/// </summary>
		FileSystem,

		/// <summary>
		/// AWS S3
		/// </summary>
		Aws,

		/// <summary>
		/// In-memory only (for testing)
		/// </summary>
		Memory,
	};

	/// <summary>
	/// Common settings for different storage backends
	/// </summary>
	public interface IStorageBackendOptions : IFileSystemStorageOptions, IAwsStorageOptions
	{
		/// <summary>
		/// The type of storage backend to use
		/// </summary>
		StorageBackendType? Type { get; }
	}

	/// <summary>
	/// Common settings object for different providers
	/// </summary>
	public class StorageBackendOptions : IStorageBackendOptions
	{
		/// <inheritdoc/>
		public StorageBackendType? Type { get; set; }

		/// <inheritdoc/>
		public string? BaseDir { get; set; }

		/// <inheritdoc/>
		public string? AwsBucketName { get; set; }

		/// <inheritdoc/>
		public string? AwsBucketPath { get; set; }

		/// <inheritdoc/>
		public AwsCredentialsType? AwsCredentials { get; set; }

		/// <inheritdoc/>
		public string? AwsRole { get; set; }

		/// <inheritdoc/>
		public string? AwsProfile { get; set; }

		/// <inheritdoc/>
		public string? AwsRegion { get; set; }
	}

	/// <summary>
	/// Options for configuring a blob store
	/// </summary>
	public class BlobStoreOptions : StorageBackendOptions
	{
	}

	/// <summary>
	/// Options for configuring the default tree store implementation
	/// </summary>
	public interface ITreeStoreOptions
	{
		/// <summary>
		/// Options for creating bundles
		/// </summary>
		BundleOptions Bundle { get; }

		/// <summary>
		/// Options for chunking content
		/// </summary>
		ChunkingOptions Chunking { get; }
	}

	/// <summary>
	/// Options for storing trees
	/// </summary>
	public class TreeStoreOptions : BlobStoreOptions, ITreeStoreOptions
	{
		/// <inheritdoc/>
		public BundleOptions Bundle { get; set; } = new BundleOptions();

		/// <inheritdoc/>
		public ChunkingOptions Chunking { get; set; } = new ChunkingOptions();
	}

	/// <summary>
	/// Authentication method used for logging users in
	/// </summary>
	public enum AuthMethod
	{
		/// <summary>
		/// No authentication enabled, mainly for demo and testing purposes
		/// </summary>
		Anonymous,

		/// <summary>
		/// OpenID Connect authentication, tailored for Okta
		/// </summary>
		Okta,
		
		/// <summary>
		/// Generic OpenID Connect authentication, recommended for most
		/// </summary>
		OpenIdConnect,
	}

	/// <summary>
	/// Type of run mode this process should use. Each carry different types of workloads. 
	/// More than one mode can be active. But not all modes are not guaranteed to be compatible with each other and will
	/// raise an error if combined in such a way.
	/// </summary>
	public enum RunMode
	{
		/// <summary>
		/// Default no-op value (ASP.NET config will default to this for enums that cannot be parsed)
		/// </summary> 
		None,
		
		/// <summary>
		/// Handle and respond to incoming external requests, such as HTTP REST and gRPC calls.
		/// These requests are time-sensitive and short-lived, typically less than 5 secs.
		/// If processes handling requests are unavailable, it will be very visible for users.
		/// </summary>
		Server,
		
		/// <summary>
		/// Run non-request facing workloads. Such as background services, processing queues, running work
		/// based on timers etc. Short periods of downtime or high CPU usage due to bursts are fine for this mode.
		/// No user requests will be impacted directly. If auto-scaling is used, a much more aggressive policy can be
		/// applied (tighter process packing, higher avg CPU usage).
		/// </summary>
		Worker
	}

	/// <summary>
	/// Type of telemetry provider to use
	/// </summary>
	public enum TelemetrySinkType
	{
		/// <summary>
		/// No telemetry sink (default)
		/// </summary>
		None,

		/// <summary>
		/// Use the Epic telemetry sink
		/// </summary>
		Epic,
		
		/// <summary>
		/// Use the ClickHouse telemetry sink
		/// </summary>
		ClickHouse,
	}

	/// <summary>
	/// Configuration for the telemetry sink
	/// </summary>
	public class BaseTelemetryConfig
	{
		/// <summary>
		/// Unique ID for this sink config (any arbitrary string)
		/// </summary>
		public string? Id { get; set; }
		
		/// <summary>
		/// Type of telemetry sink
		/// </summary>
		public TelemetrySinkType Type { get; set; } = TelemetrySinkType.None;
	}

	/// <summary>
	/// Configuration for the telemetry sink
	/// </summary>
	public class EpicTelemetryConfig : BaseTelemetryConfig, IEpicTelemetrySinkConfig
	{
		/// <summary>
		/// Base URL for the telemetry server
		/// </summary>
		public Uri? Url { get; set; }

		/// <summary>
		/// Application name to send in the event messages
		/// </summary>
		public string AppId { get; set; } = "Horde";

		/// <inheritdoc />
		public override string ToString()
		{
			return $"{nameof(Url)}={Url} {nameof(AppId)}={AppId}";
		}
	}
	
	/// <summary>
	/// Configuration for the telemetry sink
	/// </summary>
	public class ClickHouseTelemetryConfig : BaseTelemetryConfig
	{
		/// <summary>
		/// Base URL for ClickHouse server
		/// </summary>
		public Uri? Url { get; set; }
		
		/// <inheritdoc />
		public override string ToString()
		{
			return $"{nameof(Url)}={Url}";
		}
	}
	
	/// <summary>
	/// Feature flags to aid rollout of new features
	///
	/// Once a feature is running in its intended state and is stable, the flag should be removed.
	/// A name and date of when the flag was created is noted next to it to help encourage this behavior.
	/// Try having them be just a flag, a boolean.
	/// </summary>
	public class FeatureFlagSettings
	{
	}

	/// <summary>
	/// Options for the commit service
	/// </summary>
	public class CommitSettings
	{
		/// <summary>
		/// Whether to mirror commit metadata to the database
		/// </summary>
		public bool ReplicateMetadata { get; set; } = true;

		/// <summary>
		/// Whether to mirror commit data to storage
		/// </summary>
		public bool ReplicateContent { get; set; } = true;

		/// <summary>
		/// Options for how objects are packed together
		/// </summary>
		public BundleOptions Bundle { get; set; } = new BundleOptions();

		/// <summary>
		/// Options for how objects are sliced
		/// </summary>
		public ChunkingOptions Chunking { get; set; } = new ChunkingOptions();
	}

	/// <summary>
	/// Configuration for a tool bundled alongsize the server
	/// </summary>
	public class BundledToolConfig : ToolConfig
	{
		/// <summary>
		/// Version string for the current tool data
		/// </summary>
		public string Version { get; set; } = "1.0";

		/// <summary>
		/// Ref name in the tools directory
		/// </summary>
		public RefName RefName { get; set; } = new RefName("default-ref");

		/// <summary>
		/// Directory containing blob data for this tool. If empty, the tools/{id} folder next to the server will be used.
		/// </summary>
		public string? DataDir { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		public BundledToolConfig()
		{
			Public = true;
		}
	}

	/// <summary>
	/// OpenTelemetry configuration for collection and sending of traces and metrics.
	/// </summary>
	public class OpenTelemetrySettings
	{
		/// <summary>
		/// Whether OpenTelemetry exporting is enabled
		/// </summary>
		public bool Enabled { get; set; } = false;
		
		/// <summary>
		/// Service name
		/// </summary>
		public string ServiceName { get; set; } = "HordeServer";
		
		/// <summary>
		/// Service namespace
		/// </summary>
		public string ServiceNamespace { get; set; } = "Horde";
		
		/// <summary>
		/// Service version
		/// </summary>
		public string? ServiceVersion { get; set; }

		/// <summary>
		/// Whether to enrich and format telemetry to fit presentation in Datadog
		/// </summary>
		public bool EnableDatadogCompatibility { get; set; } = false;
		
		/// <summary>
		/// Extra attributes to set
		/// </summary>
		public Dictionary<string, string> Attributes { get; set; } = new();

		/// <summary>
		/// Whether to enable the console exporter (for debugging purposes)
		/// </summary>
		public bool EnableConsoleExporter { get; set; } = false;
		
		/// <summary>
		/// Protocol exporters (key is a unique and arbitrary name) 
		/// </summary>
		public Dictionary<string, OpenTelemetryProtocolExporterSettings> ProtocolExporters { get; set; } = new();
	}

	/// <summary>
	/// Configuration for an OpenTelemetry exporter
	/// </summary>
	public class OpenTelemetryProtocolExporterSettings
	{
		/// <summary>
		/// Endpoint URL. Usually differs depending on protocol used.
		/// </summary>
		public Uri? Endpoint { get; set; }

		/// <summary>
		/// Protocol for the exporter ('grpc' or 'httpprotobuf')
		/// </summary>
		public string Protocol { get; set; } = "grpc";
	}

	/// <summary>
	/// Global settings for the application
	/// </summary>
	public class ServerSettings : IAclScope
	{
		/// <inheritdoc/>
		[JsonIgnore]
		public IAclScope? ParentScope => null;

		/// <inheritdoc/>
		[JsonIgnore]
		public AclScopeName ScopeName => AclScopeName.Root;

		/// <inheritdoc cref="RunMode" />
		public RunMode[]? RunModes { get; set; } = null;

		/// <summary>
		/// Override the data directory used by Horde. Defaults to C:\ProgramData\HordeServer on Windows, {AppDir}/Data on other platforms.
		/// </summary>
		public string? DataDir { get; set; } = null;

		/// <summary>
		/// Output level for console
		/// </summary>
		public LogEventLevel ConsoleLogLevel { get; set; } = LogEventLevel.Debug;

		/// <summary>
		/// Main port for serving HTTP. Uses the default Kestrel port (5000) if not specified.
		/// </summary>
		public int HttpPort { get; set; } = 5000;

		/// <summary>
		/// Port for serving HTTP with TLS enabled.
		/// </summary>
		public int HttpsPort { get; set; } = 0;

		/// <summary>
		/// Dedicated port for serving only HTTP/2.
		/// </summary>
		public int Http2Port { get; set; }

		/// <summary>
		/// Port for listening to compute tunnel initiator requests
		/// </summary>
		public int ComputeInitiatorPort { get; set; }

		/// <summary>
		/// Port for compute remotes to connect to
		/// </summary>
		public int ComputeRemotePort { get; set; }

		/// <summary>
		/// MongoDB connection string
		/// </summary>
		public string? DatabaseConnectionString { get; set; }

		/// <summary>
		/// MongoDB database name
		/// </summary>
		public string DatabaseName { get; set; } = "Horde";

		/// <summary>
		/// Optional certificate to trust in order to access the database (eg. AWS public cert for TLS)
		/// </summary>
		public string? DatabasePublicCert { get; set; }

		/// <summary>
		/// Access the database in read-only mode (avoids creating indices or updating content)
		/// Useful for debugging a local instance of HordeServer against a production database.
		/// </summary>
		public bool DatabaseReadOnlyMode { get; set; } = false;

		/// <summary>
		/// Optional PFX certificate to use for encrypting agent SSL traffic. This can be a self-signed certificate, as long as it's trusted by agents.
		/// </summary>
		public string? ServerPrivateCert { get; set; }

		/// <summary>
		/// Issuer for tokens from the auth provider
		/// </summary>
		public AuthMethod AuthMethod { get; set; } = AuthMethod.Anonymous;

		/// <summary>
		/// Audience for OIDC validation
		/// </summary>
		public string? OidcAudience { get; set; }
		
		/// <summary>
		/// Issuer for tokens from the auth provider
		/// </summary>
		public string? OidcAuthority { get; set; }

		/// <summary>
		/// Client id for the OIDC authority
		/// </summary>
		public string? OidcClientId { get; set; }

		/// <summary>
		/// Client secret for the OIDC authority
		/// </summary>
		public string? OidcClientSecret { get; set; }

		/// <summary>
		/// Optional redirect url provided to OIDC login
		/// </summary>
		public string? OidcSigninRedirect { get; set; }

		/// <summary>
		/// Optional redirect url provided to OIDC login for external tools (typically to a local server)
		/// </summary>
		public string[]? OidcLocalRedirectUrls { get; set; }

		/// <summary>
		/// OpenID Connect scopes to request when signing in
		/// </summary>
		public string[] OidcRequestedScopes { get; set; } = { "profile", "email", "openid" };

		/// <summary>
		/// List of fields in /userinfo endpoint to try map to the standard name claim (see System.Security.Claims.ClaimTypes.Name)
		/// </summary>
		public string[] OidcClaimNameMapping { get; set; } = { "preferred_username", "email" };

		/// <summary>
		/// List of fields in /userinfo endpoint to try map to the standard email claim (see System.Security.Claims.ClaimTypes.Email)
		/// </summary>
		public string[] OidcClaimEmailMapping { get; set; } = { "email" };

		/// <summary>
		/// List of fields in /userinfo endpoint to try map to the Horde user claim (see HordeClaimTypes.User)
		/// </summary>
		public string[] OidcClaimHordeUserMapping { get; set; } = { "preferred_username", "email" };

		/// <summary>
		/// List of fields in /userinfo endpoint to try map to the Horde Perforce user claim (see HordeClaimTypes.PerforceUser)
		/// </summary>
		public string[] OidcClaimHordePerforceUserMapping { get; set; } = { "preferred_username", "email" };

		/// <summary>
		/// Name of the issuer in bearer tokens from the server
		/// </summary>
		public string? JwtIssuer { get; set; } = null!;

		/// <summary>
		/// Secret key used to sign JWTs. This setting is typically only used for development. In prod, a unique secret key will be generated and stored in the DB for each unique server instance.
		/// </summary>
		public string? JwtSecret { get; set; } = null!;

		/// <summary>
		/// Length of time before JWT tokens expire, in hours
		/// </summary>
		public int JwtExpiryTimeHours { get; set; } = 8;

		/// <summary>
		/// The claim type for administrators
		/// </summary>
		public string? AdminClaimType { get; set; }

		/// <summary>
		/// Value of the claim type for administrators
		/// </summary>
		public string? AdminClaimValue { get; set; }

		/// <summary>
		/// Whether to enable Cors, generally for development purposes
		/// </summary>
		public bool CorsEnabled { get; set; } = false;

		/// <summary>
		/// Allowed Cors origin 
		/// </summary>
		public string CorsOrigin { get; set; } = null!;

		/// <summary>
		/// Whether to automatically enable new agents by default. If false, new agents must manually be enabled before they can take on work.
		/// </summary>
		public bool EnableNewAgentsByDefault { get; set; } = true;

		/// <summary>
		/// The number of months to retain test data
		/// </summary>
		public int TestDataRetainMonths { get; set; } = 6;

		/// <summary>
		/// Interval between rebuilding the schedule queue with a DB query.
		/// </summary>
		public TimeSpan SchedulePollingInterval { get; set; } = TimeSpan.FromSeconds(60.0);

		/// <summary>
		/// Interval between polling for new jobs
		/// </summary>
		public TimeSpan NoResourceBackOffTime { get; set; } = TimeSpan.FromSeconds(30.0);

		/// <summary>
		/// Interval between attempting to assign agents to take on jobs
		/// </summary>
		public TimeSpan InitiateJobBackOffTime { get; set; } = TimeSpan.FromSeconds(180.0);

		/// <summary>
		/// Interval between scheduling jobs when an unknown error occurs
		/// </summary>
		public TimeSpan UnknownErrorBackOffTime { get; set; } = TimeSpan.FromSeconds(120.0);

		/// <summary>
		/// Config for connecting to Redis server(s).
		/// Setting it to null will disable Redis use and connection
		/// See format at https://stackexchange.github.io/StackExchange.Redis/Configuration.html
		/// </summary>
		public string? RedisConnectionConfig { get; set; }

		/// <summary>
		/// Type of write cache to use in log service
		/// Currently Supported: "InMemory" or "Redis"
		/// </summary>
		public string LogServiceWriteCacheType { get; set; } = "InMemory";

		/// <summary>
		/// Settings for artifact storage
		/// </summary>
		public StorageBackendOptions LogStorage { get; set; } = new StorageBackendOptions() { BaseDir = "Logs" };

		/// <summary>
		/// Settings for artifact storage
		/// </summary>
		public StorageBackendOptions ArtifactStorage { get; set; } = new StorageBackendOptions() { BaseDir = "Artifacts" };

		/// <summary>
		/// Configuration of tree storage
		/// </summary>
		public TreeStoreOptions CommitStorage { get; set; } = new TreeStoreOptions() { BaseDir = "Commits" };

		/// <summary>
		/// Whether to log json to stdout
		/// </summary>
		public bool LogJsonToStdOut { get; set; } = false;

		/// <summary>
		/// Whether to log requests to the UpdateSession and QueryServerState RPC endpoints
		/// </summary>
		public bool LogSessionRequests { get; set; } = false;
		
		/// <summary>
		/// Whether to enable the hosted LogService running background jobs
		/// </summary>
		public bool EnableLogService { get; set; } = true;

		/// <summary>
		/// Default fleet manager to use (when not specified by pool)
		/// </summary>
		public FleetManagerType FleetManagerV2 { get; set; } = FleetManagerType.NoOp;

		/// <summary>
		/// Config for the fleet manager (serialized JSON)
		/// </summary>
		public string? FleetManagerV2Config { get; set; }
		
		/// <summary>
		/// AWS SQS queue URL where lifecycle events from EC2 auto-scaling are received
		/// <see cref="AwsAutoScalingLifecycleService" />
		/// </summary>
		public string? AwsAutoScalingQueueUrl { get; set; }

		/// <summary>
		/// Whether to run scheduled jobs.
		/// </summary>
		public bool DisableSchedules { get; set; }

		/// <summary>
		/// Timezone for evaluating schedules
		/// </summary>
		public string? ScheduleTimeZone { get; set; }

		/// <summary>
		/// Bot token for interacting with Slack (xoxb-*)
		/// </summary>
		public string? SlackToken { get; set; }

		/// <summary>
		/// Token for opening a socket to slack (xapp-*)
		/// </summary>
		public string? SlackSocketToken { get; set; }

		/// <summary>
		/// Admin user token for Slack (xoxp-*). This is only required when using the admin endpoints to invite users.
		/// </summary>
		public string? SlackAdminToken { get; set; }

		/// <summary>
		/// Filtered list of slack users to send notifications to. Should be Slack user ids, separated by commas.
		/// </summary>
		public string? SlackUsers { get; set; }

		/// <summary>
		/// Prefix to use when reporting errors
		/// </summary>
		public string SlackErrorPrefix { get; set; } = ":horde-error: ";

		/// <summary>
		/// Prefix to use when reporting warnings
		/// </summary>
		public string SlackWarningPrefix { get; set; } = ":horde-warning: ";

		/// <summary>
		/// Channel for sending messages related to config update failures
		/// </summary>
		public string? ConfigNotificationChannel { get; set; }

		/// <summary>
		/// Channel to send stream notification update failures to
		/// </summary>
		public string? UpdateStreamsNotificationChannel { get; set; }

		/// <summary>
		/// Slack channel to send job related notifications to. Multiple channels can be specified, separated by ;
		/// </summary>
		public string? JobNotificationChannel { get; set; }

		/// <summary>
		/// The URl to use for generating links back to the dashboard.
		/// </summary>
		public Uri DashboardUrl { get; set; } = new Uri("https://localhost:3000");

		/// <summary>
		/// Help email address that users can contact with issues
		/// </summary>
		public string? HelpEmailAddress { get; set; }

		/// <summary>
		/// Help slack channel that users can use for issues
		/// </summary>
		public string? HelpSlackChannel { get; set; }

		/// <summary>
		/// Url of P4 Swarm installation
		/// </summary>
		public Uri? P4SwarmUrl { get; set; }

		/// <summary>
		/// The Jira service account user name
		/// </summary>
		public string? JiraUsername { get; set; }

		/// <summary>
		/// The Jira service account API token
		/// </summary>
		public string? JiraApiToken { get; set; }

		/// <summary>
		/// The Uri for the Jira installation
		/// </summary>
		public Uri? JiraUrl { get; set; }

		/// <summary>
		/// The number of days shared device checkouts are held
		/// </summary>
		public int SharedDeviceCheckoutDays { get; set; } = 3;

		/// <summary>
		/// Default agent pool sizing strategy for pools that doesn't have one explicitly configured
		/// </summary>
		public PoolSizeStrategy DefaultAgentPoolSizeStrategy { get; set; } = PoolSizeStrategy.LeaseUtilization;

		/// <summary>
		/// Scale-out cooldown for auto-scaling agent pools (in seconds). Can be overridden by per-pool settings.
		/// </summary>
		public int AgentPoolScaleOutCooldownSeconds { get; set; } = 60; // 1 min

		/// <summary>
		/// Scale-in cooldown for auto-scaling agent pools (in seconds). Can be overridden by per-pool settings.
		/// </summary>
		public int AgentPoolScaleInCooldownSeconds { get; set; } = 1200; // 20 mins

		/// <summary>
		/// Set the minimum size of the global thread pool
		/// This value has been found in need of tweaking to avoid timeouts with the Redis client during bursts
		/// of traffic. Default is 16 for .NET Core CLR. The correct value is dependent on the traffic the Horde Server
		/// is receiving. For Epic's internal deployment, this is set to 40.
		/// </summary>
		public int? GlobalThreadPoolMinSize { get; set; }

		/// <summary>
		/// Whether to enable Datadog integration for tracing
		/// </summary>
		public bool WithDatadog { get; set; }

		/// <summary>
		/// Whether to enable Amazon Web Services (AWS) specific features
		/// </summary>
		public bool WithAws { get; set; } = false;

		/// <summary>
		/// Path to the root config file
		/// </summary>
		public string ConfigPath { get; set; } = "Defaults/globals.json";

		/// <summary>
		/// Perforce connections for use by the Horde server (not agents)
		/// </summary>
		public List<PerforceConnectionSettings> Perforce { get; set; } = new List<PerforceConnectionSettings>();

		/// <summary>
		/// Whether to use the local Perforce environment
		/// </summary>
		public bool UseLocalPerforceEnv { get; set; }

		/// <summary>
		/// Number of pooled perforce connections to keep
		/// </summary>
		public int PerforceConnectionPoolSize { get; set; } = 5;

		/// <summary>
		/// Whether to enable the upgrade task source.
		/// </summary>
		public bool EnableUpgradeTasks { get; set; } = true;

		/// <summary>
		/// Whether to enable the conform task source.
		/// </summary>
		public bool EnableConformTasks { get; set; } = true;

		/// <summary>
		/// Forces configuration data to be read and updated as part of appplication startup, rather than on a schedule. Useful when running locally.
		/// </summary>
		public bool ForceConfigUpdateOnStartup { get; set; }

		/// <summary>
		/// Whether to open a browser on startup
		/// </summary>
		public bool OpenBrowser { get; set; } = false;

		/// <inheritdoc cref="FeatureFlags" />
		public FeatureFlagSettings FeatureFlags { get; set; } = new ();

		/// <summary>
		/// Options for the commit service
		/// </summary>
		public CommitSettings Commits { get; set; } = new CommitSettings();

		/// <summary>
		/// Settings for sending telemetry events to external services (for example Snowflake, ClickHouse etc)
		/// </summary>
		public List<BaseTelemetryConfig> Telemetry { get; set; } = new ();

		/// <summary>
		/// Tools bundled along with the server. Data for each tool can be produced using the 'bundle create' command, and should be stored in the /tools/{id} directory.
		/// </summary>
		public List<BundledToolConfig> BundledTools { get; set; } = new List<BundledToolConfig>();
		
		/// <summary>
		/// Options for OpenTelemetry
		/// </summary>
		public OpenTelemetrySettings OpenTelemetry { get; set; } = new OpenTelemetrySettings();

		/// <summary>
		/// Default pre-baked ACL for authentication of well-known roles
		/// </summary>
		[JsonIgnore]
		public AclConfig? Acl
		{
			get
			{
				_defaultAcl ??= GetDefaultAcl();
				return _defaultAcl;
			}
		}
		
		[JsonIgnore]
		AclConfig? _defaultAcl;

		/// <summary>
		/// Create the default ACL for the server, including all predefined roles.
		/// </summary>
		/// <returns></returns>
		static AclConfig GetDefaultAcl()
		{
			AclConfig defaultAcl = new AclConfig();
			defaultAcl.Entries.Add(new AclEntryConfig(new AclClaimConfig(ClaimTypes.Role, "internal:AgentRegistration"), new[] { AgentAclAction.CreateAgent, SessionAclAction.CreateSession }));
			defaultAcl.Entries.Add(new AclEntryConfig(HordeClaims.AgentRegistrationClaim, new[] { AgentAclAction.CreateAgent, SessionAclAction.CreateSession, AgentAclAction.UpdateAgent, AgentSoftwareAclAction.DownloadSoftware, PoolAclAction.CreatePool, PoolAclAction.UpdatePool, PoolAclAction.ViewPool, PoolAclAction.DeletePool, PoolAclAction.ListPools, StreamAclAction.ViewStream, ProjectAclAction.ViewProject, JobAclAction.ViewJob, ServerAclAction.ViewCosts }));
			defaultAcl.Entries.Add(new AclEntryConfig(HordeClaims.AgentRoleClaim, new[] { ProjectAclAction.ViewProject, StreamAclAction.ViewStream, LogAclAction.CreateEvent, AgentSoftwareAclAction.DownloadSoftware }));
			defaultAcl.Entries.Add(new AclEntryConfig(HordeClaims.DownloadSoftwareClaim, new[] { AgentSoftwareAclAction.DownloadSoftware }));
			defaultAcl.Entries.Add(new AclEntryConfig(HordeClaims.UploadSoftwareClaim, new[] { AgentSoftwareAclAction.UploadSoftware }));
			defaultAcl.Entries.Add(new AclEntryConfig(HordeClaims.ConfigureProjectsClaim, new[] { ProjectAclAction.CreateProject, ProjectAclAction.UpdateProject, ProjectAclAction.ViewProject, StreamAclAction.CreateStream, StreamAclAction.UpdateStream, StreamAclAction.ViewStream }));
			defaultAcl.Entries.Add(new AclEntryConfig(HordeClaims.StartChainedJobClaim, new[] { JobAclAction.CreateJob, JobAclAction.ExecuteJob, JobAclAction.UpdateJob, JobAclAction.ViewJob, StreamAclAction.ViewTemplate, StreamAclAction.ViewStream }));
			return defaultAcl;
		}

		/// <summary>
		/// Helper method to check if this process has activated the given mode
		/// </summary>
		/// <param name="mode">Run mode</param>
		/// <returns>True if mode is active</returns>
		public bool IsRunModeActive(RunMode mode)
		{
			if (RunModes == null)
			{
				return true;
			}
			return RunModes.Contains(mode);
		}

		/// <summary>
		/// Validate the settings object does not contain any invalid fields
		/// </summary>
		/// <exception cref="ArgumentException"></exception>
		public void Validate()
		{
			if (RunModes != null && IsRunModeActive(RunMode.None))
			{
				throw new ArgumentException($"Settings key '{nameof(RunModes)}' contains one or more invalid entries");
			}
		}

		/// <summary>
		/// Attempts to get a bundled tool with the given id
		/// </summary>
		/// <param name="toolId">The tool id</param>
		/// <param name="bundledToolConfig">Configuration for the bundled tool</param>
		/// <returns>True if the tool was found</returns>
		public bool TryGetBundledTool(ToolId toolId, [NotNullWhen(true)] out BundledToolConfig? bundledToolConfig)
		{
			bundledToolConfig = BundledTools.FirstOrDefault(x => x.Id == toolId);
			return bundledToolConfig != null;
		}
	}

	/// <summary>
	/// Identifier for a pool
	/// </summary>
	/// <param name="Id">Id to construct from</param>
	[JsonSchemaString]
	[StringIdConverter(typeof(PerforceConnectionIdConverter))]
	public record struct PerforceConnectionId(StringId Id)
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public PerforceConnectionId(string id) : this(new StringId(id))
		{
		}

		/// <inheritdoc/>
		public override string ToString() => Id.ToString();
	}

	/// <summary>
	/// Converter to and from <see cref="StringId"/> instances.
	/// </summary>
	class PerforceConnectionIdConverter : StringIdConverter<PerforceConnectionId>
	{
		/// <inheritdoc/>
		public override PerforceConnectionId FromStringId(StringId id) => new PerforceConnectionId(id);

		/// <inheritdoc/>
		public override StringId ToStringId(PerforceConnectionId value) => value.Id;
	}

	/// <summary>
	/// Perforce connection information for use by the Horde server (for reading config files, etc...)
	/// </summary>
	public class PerforceConnectionSettings
	{
		/// <summary>
		/// Identifier for the default perforce connection profile
		/// </summary>
		public static PerforceConnectionId Default { get; } = new PerforceConnectionId("default");

		/// <summary>
		/// Identifier for this server
		/// </summary>
		public PerforceConnectionId Id { get; set; } = Default;

		/// <summary>
		/// Server and port
		/// </summary>
		public string? ServerAndPort { get; set; }

		/// <summary>
		/// Credentials for the server
		/// </summary>
		public PerforceCredentials? Credentials { get; set; }

		/// <summary>
		/// Create a <see cref="PerforceSettings"/> object with these settings as overrides
		/// </summary>
		/// <returns>New perforce settings object</returns>
		public PerforceSettings ToPerforceSettings()
		{
			PerforceSettings settings = new PerforceSettings(PerforceSettings.Default);
			settings.PreferNativeClient = true;

			if (!String.IsNullOrEmpty(ServerAndPort))
			{
				settings.ServerAndPort = ServerAndPort;
			}
			if (Credentials != null)
			{
				if (!String.IsNullOrEmpty(Credentials.UserName))
				{
					settings.UserName = Credentials.UserName;
				}
				if (!String.IsNullOrEmpty(Credentials.Password))
				{
					settings.Password = Credentials.Password;
				}
			}
			return settings;
		}
	}
}
