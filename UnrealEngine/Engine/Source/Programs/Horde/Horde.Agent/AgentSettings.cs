// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel.DataAnnotations;
using EpicGames.Core;
using EpicGames.Horde;
using Microsoft.Extensions.Configuration;

namespace Horde.Agent
{
	/// <summary>
	/// Describes a network share to mount
	/// </summary>
	public class MountNetworkShare
	{
		/// <summary>
		/// Where the share should be mounted on the local machine. Must be a drive letter for Windows.
		/// </summary>
		public string? MountPoint { get; set; }

		/// <summary>
		/// Path to the remote resource
		/// </summary>
		public string? RemotePath { get; set; }
	}

	/// <summary>
	/// Information about a server to use
	/// </summary>
	public class ServerProfile
	{
		/// <summary>
		/// Name of this server profile
		/// </summary>
		public string? Name { get; set; }

		/// <summary>
		/// Name of the environment (currently just used for tracing)
		/// </summary>
		[Required]
		public string Environment { get; set; } = "prod";

		/// <summary>
		/// Url of the server
		/// </summary>
		[Required]
		public Uri Url { get; set; } = null!;

		/// <summary>
		/// Bearer token to use to initiate the connection
		/// </summary>
		public string? Token { get; set; }

		/// <summary>
		/// Thumbprint of a certificate to trust. Allows using self-signed certs for the server.
		/// </summary>
		public string? Thumbprint { get; set; }

		/// <summary>
		/// Thumbprints of certificates to trust. Allows using self-signed certs for the server.
		/// </summary>
		public List<string> Thumbprints { get; } = new List<string>();

		/// <summary>
		/// Checks whether the given certificate thumbprint should be trusted
		/// </summary>
		/// <param name="certificateThumbprint">The cert thumbprint</param>
		/// <returns>True if the cert should be trusted</returns>
		public bool IsTrustedCertificate(string certificateThumbprint)
		{
			if (Thumbprint != null && Thumbprint.Equals(certificateThumbprint, StringComparison.OrdinalIgnoreCase))
			{
				return true;
			}
			if (Thumbprints.Any(x => x.Equals(certificateThumbprint, StringComparison.OrdinalIgnoreCase)))
			{
				return true;
			}
			return false;
		}
	}

	/// <summary>
	/// Settings for the local executor
	/// </summary>
	public class LocalExecutorSettings
	{
		/// <summary>
		/// Path to the local workspace to use with the local executor
		/// </summary>
		public string? WorkspaceDir { get; set; }

		/// <summary>
		/// Whether to actually execute steps, or just do job setup
		/// </summary>
		public bool RunSteps { get; set; } = true;
	}

	/// <summary>
	/// Settings for the perforce executor
	/// </summary>
	public class PerforceExecutorSettings
	{
		/// <summary>
		/// Whether to run conform jobs
		/// </summary>
		public bool RunConform { get; set; } = true;
	}

	/// <summary>
	/// Flags for processes to terminate
	/// </summary>
	[Flags]
	public enum TerminateCondition
	{
		/// <summary>
		/// Not specified; terminate in all circumstances
		/// </summary>
		None = 0,

		/// <summary>
		/// When a session starts
		/// </summary>
		BeforeSession = 1,

		/// <summary>
		/// Before running a conform
		/// </summary>
		BeforeConform = 2,

		/// <summary>
		/// Before executing a batch
		/// </summary>
		BeforeBatch = 4,

		/// <summary>
		/// Terminate at the end of a batch
		/// </summary>
		AfterBatch = 8,

		/// <summary>
		/// After a step completes
		/// </summary>
		AfterStep = 16,
	}

	/// <summary>
	/// Specifies a process to terminate
	/// </summary>
	public class ProcessToTerminate
	{
		/// <summary>
		/// Name of the process
		/// </summary>
		public string Name { get; set; } = String.Empty;

		/// <summary>
		/// When to terminate this process
		/// </summary>
		public List<TerminateCondition>? When { get; init; }
	}

	/// <summary>
	/// Global settings for the agent
	/// </summary>
	public class AgentSettings
	{
		/// <summary>
		/// Name of the section containing these settings
		/// </summary>
		public const string SectionName = "Horde";

		/// <summary>
		/// Known servers to connect to
		/// </summary>
		public Dictionary<string, ServerProfile> ServerProfiles { get; } = new Dictionary<string, ServerProfile>(StringComparer.OrdinalIgnoreCase);

		/// <summary>
		/// The default server, unless overridden from the command line
		/// </summary>
		public string? Server { get; set; }

		/// <summary>
		/// Name of agent to report as when connecting to server.
		/// By default, the computer's hostname will be used.
		/// </summary>
		public string? Name { get; set; }

		/// <summary>
		/// Whether the server is running in 'installed' mode. In this mode, on Windows, the default data directory will use the common 
		/// application data folder (C:\ProgramData\Epic\Horde), and configuration data will be read from here and the registry.
		/// This setting is overridden to false for local builds from appsettings.Local.json.
		/// </summary>
		public bool Installed { get; set; } = true;

		/// <summary>
		/// Whether agent should register as being ephemeral.
		/// Doing so will not persist any long-lived data on the server and
		/// once disconnected it's assumed to have been deleted permanently.
		/// Ideal for short-lived agents, such as spot instances on AWS EC2.
		/// </summary>
		public bool Ephemeral { get; set; } = false;

		/// <summary>
		/// The executor to use for jobs
		/// </summary>
		public string Executor { get; set; } = Execution.WorkspaceExecutor.Name;

		/// <summary>
		/// Settings for the local executor
		/// </summary>
		public LocalExecutorSettings LocalExecutor { get; set; } = new LocalExecutorSettings();

		/// <summary>
		/// Settings for the perforce executor
		/// </summary>
		public PerforceExecutorSettings PerforceExecutor { get; set; } = new PerforceExecutorSettings();

		/// <summary>
		/// Working directory
		/// </summary>
		public DirectoryReference WorkingDir { get; set; } = DirectoryReference.Combine(AgentApp.DataDir, "Sandbox");

		/// <summary>
		/// Whether to mount the specified list of network shares
		/// </summary>
		public bool ShareMountingEnabled { get; set; } = true;

		/// <summary>
		/// List of network shares to mount
		/// </summary>
		public List<MountNetworkShare> Shares { get; } = new List<MountNetworkShare>();

		/// <summary>
		/// List of process names to terminate after a job
		/// </summary>
		[Obsolete("Prefer using the ProcessesToTerminate list instead")]
		public List<string> ProcessNamesToTerminate { get; } = new List<string>();

		/// <summary>
		/// List of process names to terminate after a lease completes, but not after a job step
		/// </summary>
		public List<ProcessToTerminate> ProcessesToTerminate { get; } = new List<ProcessToTerminate>();

		/// <summary>
		/// Path to Wine executable. If null, execution under Wine is disabled
		/// </summary>
		public string? WineExecutablePath { get; set; }

		/// <summary>
		/// Path to container engine executable, such as /usr/bin/podman. If null, execution of compute workloads inside a container is disabled
		/// </summary>
		public string? ContainerEngineExecutablePath { get; set; }

		/// <summary>
		/// Whether to write step output to the logging device
		/// </summary>
		public bool WriteStepOutputToLogger { get; set; }

		/// <summary>
		/// Queries information about the current agent through the AWS EC2 interface
		/// </summary>
		public bool EnableAwsEc2Support { get; set; } = true;

		/// <summary>
		/// Option to use a local storage client rather than connecting through the server. Primarily for convenience when debugging / iterating locally.
		/// </summary>
		public bool UseLocalStorageClient { get; set; }

		/// <summary>
		/// Incoming port for listening for compute work. Needs to be tied with a lease.
		/// </summary>
		public int ComputePort { get; set; } = 7000;

		/// <summary>
		/// Whether to send telemetry back to Horde server
		/// </summary>
		public bool EnableTelemetry { get; set; } = false;

		/// <summary>
		/// How often to report telemetry events to server in milliseconds
		/// </summary>
		public int TelemetryReportInterval { get; set; } = 30 * 1000;

		/// <summary>
		/// Maximum size of the bundle cache, in megabytes.
		/// </summary>
		public long BundleCacheSize { get; set; } = 1024;

		/// <summary>
		/// Key/value properties in addition to those set internally by the agent
		/// </summary>
		public Dictionary<string, string> Properties { get; } = new();

		/// <summary>
		/// Gets the current server settings
		/// </summary>
		/// <returns>The current server settings</returns>
		public ServerProfile GetServerProfile(string name)
		{
			ServerProfile? serverProfile;
			if (!ServerProfiles.TryGetValue(name, out serverProfile))
			{
				serverProfile = ServerProfiles.Values.FirstOrDefault(x => name.Equals(x.Name, StringComparison.OrdinalIgnoreCase));
				if (serverProfile == null)
				{
					if (ServerProfiles.Count == 0)
					{
						throw new Exception("No server profiles are defined (missing configuration?)");
					}
					else
					{
						throw new Exception($"Unknown server profile name '{name}' (valid profiles: {GetServerProfileNames()})");
					}
				}
			}
			return serverProfile;
		}

		string GetServerProfileNames()
		{
			HashSet<string> names = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
			foreach ((string key, ServerProfile profile) in ServerProfiles)
			{
				if (!String.IsNullOrEmpty(profile.Name) && Int32.TryParse(key, out _))
				{
					names.Add(profile.Name);
				}
				else
				{
					names.Add(key);
				}
			}
			return String.Join("/", names);
		}

		/// <summary>
		/// Gets the current server settings
		/// </summary>
		/// <returns>The current server settings</returns>
		public ServerProfile GetCurrentServerProfile()
		{
			if (Server == null)
			{
				Uri? defaultServerUrl = Installed ? HordeOptions.GetDefaultServerUrl() : null;

				ServerProfile defaultServerProfile = new ServerProfile();
				defaultServerProfile.Name = "Default";
				defaultServerProfile.Environment = "Development";
				defaultServerProfile.Url = defaultServerUrl ?? new Uri("http://localhost:5000");
				return defaultServerProfile;
			}

			return GetServerProfile(Server);
		}

		/// <summary>
		/// Gets a lookup of process name to the circumstances in which it should be terminate
		/// </summary>
		public Dictionary<string, TerminateCondition> GetProcessesToTerminateMap()
		{
			Dictionary<string, TerminateCondition> processesToTerminate = new Dictionary<string, TerminateCondition>(StringComparer.OrdinalIgnoreCase);
#pragma warning disable CS0618 // Type or member is obsolete
			foreach (string processName in ProcessNamesToTerminate)
			{
				processesToTerminate[processName] = TerminateCondition.None;
			}
#pragma warning restore CS0618 // Type or member is obsolete
			foreach (ProcessToTerminate processToTerminate in ProcessesToTerminate)
			{
				TerminateCondition condition = default;
				if (processToTerminate.When != null)
				{
					foreach (TerminateCondition when in processToTerminate.When)
					{
						condition |= when;
					}
				}
				processesToTerminate[processToTerminate.Name] = condition;
			}
			return processesToTerminate;
		}

		/// <summary>
		/// Path to file used for signaling impending termination and shutdown of the agent
		/// </summary>
		/// <returns>Path to file which may or may not exist</returns>
		public FileReference GetTerminationSignalFile()
		{
			return FileReference.Combine(WorkingDir, ".horde-termination-signal");
		}

		internal string GetAgentName()
		{
			return Name ?? Environment.MachineName;
		}
	}

	/// <summary>
	/// Extension methods for retrieving config settings
	/// </summary>
	public static class AgentSettingsExtensions
	{
		/// <summary>
		/// Gets the configuration section for the active server profile
		/// </summary>
		/// <param name="configSection"></param>
		/// <returns></returns>
		public static IConfigurationSection GetCurrentServerProfile(this IConfigurationSection configSection)
		{
			string? profileName = configSection[nameof(AgentSettings.Server)];
			if (profileName == null)
			{
				throw new Exception("Server is not set");
			}

			return configSection.GetSection(nameof(AgentSettings.ServerProfiles)).GetChildren().First(x => x[nameof(ServerProfile.Name)] == profileName);
		}
	}
}
