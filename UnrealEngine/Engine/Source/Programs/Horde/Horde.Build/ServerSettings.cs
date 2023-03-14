// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Linq;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Nodes;
using Horde.Build.Agents.Fleet;
using Horde.Build.Storage;
using Horde.Build.Storage.Backends;
using Horde.Build.Utilities;
using TimeZoneConverter;

namespace Horde.Build
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
		Transient,

		/// <summary>
		/// Relay to another server (useful for testing against prod)
		/// </summary>
		Relay,
	};

	/// <summary>
	/// Common settings for different storage backends
	/// </summary>
	public interface IStorageBackendOptions : IFileSystemStorageOptions, IAwsStorageOptions, IRelayStorageOptions
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
		public AwsCredentialsType AwsCredentials { get; set; }

		/// <inheritdoc/>
		public string? AwsRole { get; set; }

		/// <inheritdoc/>
		public string? AwsProfile { get; set; }

		/// <inheritdoc/>
		public string? AwsRegion { get; set; }

		/// <inheritdoc/>
		public string? RelayServer { get; set; }

		/// <inheritdoc/>
		public string? RelayToken { get; set; }
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
	/// Specifies the service to use for controlling the size of the fleet
	/// </summary>
	public enum FleetManagerType
	{
		/// <summary>
		/// Default (empty) instance
		/// </summary>
		None,

		/// <summary>
		/// Use AWS EC2 instances
		/// </summary>
		Aws,
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
	/// Global settings for the application
	/// </summary>
	public class ServerSettings
	{
		/// <inheritdoc cref="RunMode" />
		public RunMode[]? RunModes { get; set; } = null;
		
		/// <summary>
		/// Main port for serving HTTP. Uses the default Kestrel port (5000) if not specified.
		/// </summary>
		public int HttpPort { get; set; }

		/// <summary>
		/// Port for serving HTTP with TLS enabled. Uses the default Kestrel port (5001) if not specified.
		/// </summary>
		public int HttpsPort { get; set; }

		/// <summary>
		/// Dedicated port for serving only HTTP/2.
		/// </summary>
		public int Http2Port { get; set; }

		/// <summary>
		/// Whether the server is running as a single instance or with multiple instances, such as in k8s
		/// </summary>
		public bool SingleInstance { get; set; } = false;

		/// <summary>
		/// MongoDB connection string
		/// </summary>
		public string? DatabaseConnectionString { get; set; }

		/// <summary>
		/// MongoDB database name
		/// </summary>
		public string DatabaseName { get; set; } = "Horde";

		/// <summary>
		/// The claim type for administrators
		/// </summary>
		public string AdminClaimType { get; set; } = HordeClaimTypes.InternalRole;

		/// <summary>
		/// Value of the claim type for administrators
		/// </summary>
		public string AdminClaimValue { get; set; } = "admin";

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
		/// Length of time before JWT tokens expire, in hourse
		/// </summary>
		public int JwtExpiryTimeHours { get; set; } = 4;

		/// <summary>
		/// Whether to enable Cors, generally for development purposes
		/// </summary>
		public bool CorsEnabled { get; set; } = false;

		/// <summary>
		/// Allowed Cors origin 
		/// </summary>
		public string CorsOrigin { get; set; } = null!;

		/// <summary>
		/// Whether to enable a schedule in test data (false by default for development builds)
		/// </summary>
		public bool EnableScheduleInTestData { get; set; }

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
		/// Which fleet manager service to use
		/// </summary>
		public FleetManagerType FleetManager { get; set; } = FleetManagerType.None;

		/// <summary>
		/// Whether to run scheduled jobs.
		/// </summary>
		public bool DisableSchedules { get; set; }

		/// <summary>
		/// Timezone for evaluating schedules
		/// </summary>
		public string? ScheduleTimeZone { get; set; }

		/// <summary>
		/// Token for interacting with Slack
		/// </summary>
		public string? SlackToken { get; set; }

		/// <summary>
		/// Token for opening a socket to slack
		/// </summary>
		public string? SlackSocketToken { get; set; }

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
		/// Channel to send stream notification update failures to
		/// </summary>
		public string? UpdateStreamsNotificationChannel { get; set; }

		/// <summary>
		/// Channel to send device notifications to
		/// </summary>
		public string? DeviceServiceNotificationChannel { get; set; }
		
		/// <summary>
		/// Slack channel to send job related notifications to
		/// </summary>
		public string? JobNotificationChannel { get; set; }

		/// <summary>
		/// URI to the SmtpServer to use for sending email notifications
		/// </summary>
		public string? SmtpServer { get; set; }

		/// <summary>
		/// The email address to send email notifications from
		/// </summary>
		public string? EmailSenderAddress { get; set; }

		/// <summary>
		/// The name for the sender when sending email notifications
		/// </summary>
		public string? EmailSenderName { get; set; }

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
		/// The p4 bridge server
		/// </summary>
		public string? P4BridgeServer { get; set; }

		/// <summary>
		/// The p4 bridge service username
		/// </summary>
		public string? P4BridgeServiceUsername { get; set; }

		/// <summary>
		/// The p4 bridge service password
		/// </summary>
		public string? P4BridgeServicePassword { get; set; }

		/// <summary>
		/// Whether the p4 bridge service account can impersonate other users
		/// </summary>
		public bool P4BridgeCanImpersonate { get; set; } = false;

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
		/// Path to the root config file
		/// </summary>
		public string ConfigPath { get; set; } = "Defaults/globals.json";

		/// <summary>
		/// Settings for the storage service
		/// </summary>
		public StorageOptions? Storage { get; set; }

		/// <summary>
		/// Namespace to use for storing tools
		/// </summary>
		public NamespaceId ToolNamespaceId { get; set; } = new NamespaceId("horde.p4");

		/// <summary>
		/// Whether to 
		/// </summary>
		public bool UseLocalPerforceEnv { get; set; }

		/// <summary>
		/// Lazily computed timezone value
		/// </summary>
		public TimeZoneInfo TimeZoneInfo
		{
			get
			{
				if (_cachedTimeZoneInfo == null)
				{
					_cachedTimeZoneInfo = (ScheduleTimeZone == null) ? TimeZoneInfo.Local : TZConvert.GetTimeZoneInfo(ScheduleTimeZone);
				}
				return _cachedTimeZoneInfo;
			}
		}

		private TimeZoneInfo? _cachedTimeZoneInfo;

		/// <summary>
		/// Whether to open a browser on startup
		/// </summary>
		public bool OpenBrowser { get; set; } = false;

		/// <inheritdoc cref="FeatureFlags" />
		public FeatureFlagSettings FeatureFlags { get; set; } = new ();

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
	}
}
