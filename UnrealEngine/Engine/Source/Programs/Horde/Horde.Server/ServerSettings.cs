// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Net;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Server;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Nodes;
using EpicGames.Horde.Tools;
using EpicGames.Perforce;
using Horde.Server.Agents.Fleet;
using Horde.Server.Server;
using Horde.Server.Storage.ObjectStores;
using Horde.Server.Telemetry.Sinks;
using Horde.Server.Tools;

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
		/// Azure blob store
		/// </summary>
		Azure,

		/// <summary>
		/// In-memory only (for testing)
		/// </summary>
		Memory,
	};

	/// <summary>
	/// Common settings for different storage backends
	/// </summary>
	public interface IStorageBackendOptions : IAwsStorageOptions, IAzureStorageOptions
	{
		/// <summary>
		/// Base directory for filesystem storage
		/// </summary>
		string? BaseDir { get; }

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

		/// <inheritdoc/>
		public string? AzureConnectionString { get; set; }

		/// <inheritdoc/>
		public string? AzureContainerName { get; set; }
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

		/// <summary>
		/// Mongo telemetry
		/// </summary>
		Mongo,
	}

	/// <summary>
	/// Configuration for the telemetry sink
	/// </summary>
	public class BaseTelemetryConfig
	{
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
	/// Configuration for the telemetry sink
	/// </summary>
	public class MongoTelemetryConfig : BaseTelemetryConfig
	{
		/// <summary>
		/// Number of days worth of telmetry events to keep
		/// </summary>
		public double RetainDays { get; set; } = 1;
	}

	/// <summary>
	/// Feature flags to aid rollout of new features.
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
	public class ServerSettings
	{
		/// <summary>
		/// Name of the section containing these settings
		/// </summary>
		public const string SectionName = "Horde";

		/// <summary>
		/// Modes that the server should run in. Runmodes can be used in a multi-server deployment to limit the operations that a particular instance will try to perform.
		/// </summary>
		public RunMode[]? RunModes { get; set; } = null;

		/// <summary>
		/// Override the data directory used by Horde. Defaults to C:\ProgramData\HordeServer on Windows, {AppDir}/Data on other platforms.
		/// </summary>
		public string? DataDir { get; set; } = null;

		/// <summary>
		/// Whether the server is running in 'installed' mode. In this mode, on Windows, the default data directory will use the common 
		/// application data folder (C:\ProgramData\Epic\Horde), and configuration data will be read from here and the registry.
		/// This setting is overridden to false for local builds from appsettings.Local.json.
		/// </summary>
		public bool Installed { get; set; } = true;

		/// <summary>
		/// Main port for serving HTTP.
		/// </summary>
		public int HttpPort { get; set; } = 5000;

		/// <summary>
		/// Port for serving HTTP with TLS enabled. Disabled by default.
		/// </summary>
		public int HttpsPort { get; set; } = 0;

		/// <summary>
		/// Dedicated port for serving only HTTP/2.
		/// </summary>
		public int Http2Port { get; set; } = 5002;

		/// <summary>
		/// Port to listen on for tunneling compute sockets to agents
		/// </summary>
		public int ComputeTunnelPort { get; set; }

		/// <summary>
		/// What address (host:port) clients should connect to for compute socket tunneling
		/// Port may differ from <see cref="ComputeTunnelPort" /> if Horde server is behind a reverse proxy/firewall
		/// </summary>
		public string? ComputeTunnelAddress { get; set; }

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
		/// Shutdown the current server process if memory usage reaches this threshold (specified in MB)
		///
		/// Usually set to 80-90% of available memory to avoid CLR heap using all of it.
		/// If a memory leak was to occur, it's usually better to restart the process rather than to let the GC
		/// work harder and harder trying to recoup memory.
		/// 
		/// Should only be used when multiple server processes are running behind a load balancer
		/// and one can be safely restarted automatically by the underlying process handler (Docker, Kubernetes, AWS ECS, Supervisor etc).
		/// The shutdown behaves similar to receiving a SIGTERM and will wait for outstanding requests to finish.
		/// </summary>
		public int? ShutdownMemoryThreshold { get; set; } = null;

		/// <summary>
		/// Optional PFX certificate to use for encrypting agent SSL traffic. This can be a self-signed certificate, as long as it's trusted by agents.
		/// </summary>
		public string? ServerPrivateCert { get; set; }

		/// <summary>
		/// Issuer for tokens from the auth provider
		/// </summary>
		public AuthMethod AuthMethod { get; set; } = AuthMethod.Anonymous;

		/// <summary>
		/// Optional profile name to report through the /api/v1/server/auth endpoint. Allows sharing auth tokens between providers configured through
		/// the same profile name in OidcToken.exe config files.
		/// </summary>
		public string? OidcProfileName { get; set; }

		/// <summary>
		/// Issuer for tokens from the auth provider
		/// </summary>
		public string? OidcAuthority { get; set; }

		/// <summary>
		/// Audience for validating externally issued tokens
		/// </summary>
		public string? OidcAudience { get; set; }

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
		public string[]? OidcLocalRedirectUrls { get; set; } =
		{
			"http://localhost:8749/ugs.client"
		};

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
		/// Name of this machine 
		/// </summary>
		public Uri ServerUrl
		{
			get => _serverUrl ?? GetDefaultServerUrl();
			set => _serverUrl = value;
		}

		/// <summary>
		/// Name of the issuer in bearer tokens from the server
		/// </summary>
		public string? JwtIssuer
		{
			get => _jwtIssuer ?? ServerUrl.ToString();
			set => _jwtIssuer = value;
		}

		Uri? _serverUrl;
		string? _jwtIssuer;

		Uri GetDefaultServerUrl()
		{
			string hostName = Dns.GetHostName();
			if (HttpsPort == 443)
			{
				return new Uri($"https://{hostName}");
			}
			else if (HttpsPort != 0)
			{
				return new Uri($"https://{hostName}:{HttpsPort}");
			}
			else if (HttpPort == 80)
			{
				return new Uri($"http://{hostName}");
			}
			else
			{
				return new Uri($"http://{hostName}:{HttpPort}");
			}
		}

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
		public bool EnableNewAgentsByDefault { get; set; } = false;

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
		/// AWS SQS queue URLs where lifecycle events from EC2 auto-scaling are received
		/// <see cref="AwsAutoScalingLifecycleService" />
		/// </summary>
		public string[] AwsAutoScalingQueueUrls { get; set; } = Array.Empty<string>();

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
		/// Slack channel to send agent related notifications to.
		/// </summary>
		public string? AgentNotificationChannel { get; set; }

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
		/// The number of cooldown minutes for device problems
		/// </summary>
		public int DeviceProblemCooldownMinutes { get; set; } = 10;

		/// <summary>
		/// Channel to send device reports to
		/// </summary>
		public string? DeviceReportChannel { get; set; }

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
		/// Path to the root config file. Relative to the server.json file by default.
		/// </summary>
		public string ConfigPath { get; set; } = "globals.json";

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

		/// <summary>
		/// Directory to use for cache data
		/// </summary>
		public string? BundleCacheDir { get; set; }

		/// <summary>
		/// Maximum size of the storage cache on disk, in megabytes
		/// </summary>
		public long BundleCacheSize { get; set; } = 1024;

		/// <summary>
		/// Experimental features to enable on the server.
		/// </summary>
		public FeatureFlagSettings FeatureFlags { get; set; } = new();

		/// <summary>
		/// Options for the commit service
		/// </summary>
		public CommitSettings Commits { get; set; } = new CommitSettings();

		/// <summary>
		/// Settings for sending telemetry events to external services (for example Snowflake, ClickHouse etc)
		/// </summary>
		public List<BaseTelemetryConfig> Telemetry { get; set; } = new();

		/// <summary>
		/// Tools bundled along with the server. Data for each tool can be produced using the 'bundle create' command, and should be stored in the Tools directory.
		/// </summary>
		public List<BundledToolConfig> BundledTools { get; set; } = new List<BundledToolConfig>();

		/// <summary>
		/// Options for OpenTelemetry
		/// </summary>
		public OpenTelemetrySettings OpenTelemetry { get; set; } = new OpenTelemetrySettings();

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

				if (!String.IsNullOrEmpty(Credentials.Ticket))
				{
					settings.Password = Credentials.Ticket;
				}
				else if (!String.IsNullOrEmpty(Credentials.Password))
				{
					settings.Password = Credentials.Password;
				}
			}
			return settings;
		}
	}
}
