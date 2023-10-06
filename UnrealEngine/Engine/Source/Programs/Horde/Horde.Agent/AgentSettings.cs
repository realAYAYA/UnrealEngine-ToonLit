// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Linq;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using Horde.Agent.Services;
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
		[Required]
		public string Name { get; set; } = null!;

		/// <summary>
		/// Name of the environment (currrently just used for tracing)
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
	/// Setttings for the perforce executor
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
		public List<ServerProfile> ServerProfiles { get; } = new List<ServerProfile>();

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
		/// The executor to use for jobs. Defaults to the Perforce executor.
		/// </summary>
		public string Executor { get; set; } = Execution.PerforceExecutor.Name;

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
		public string? WorkingDir { get; set; } = DirectoryReference.Combine(Program.DataDir, "Data").FullName;

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
		/// How often to report telemetry events to server in milliseconds
		/// </summary>
		public int TelemetryReportInterval { get; set; } = 30 * 1000;

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
			ServerProfile? serverProfile = ServerProfiles.FirstOrDefault(x => x.Name.Equals(name, StringComparison.OrdinalIgnoreCase));
			if (serverProfile == null)
			{
				if (ServerProfiles.Count == 0)
				{
					throw new Exception("No server profiles are defined (missing configuration?)");
				}
				else
				{
					throw new Exception($"Unknown server profile name '{name}' (valid profiles: {String.Join("/", ServerProfiles.Select(x => x.Name))})");
				}
			}
			return serverProfile;
		}

		/// <summary>
		/// Gets the current server settings
		/// </summary>
		/// <returns>The current server settings</returns>
		public ServerProfile GetCurrentServerProfile()
		{
			if (Server == null)
			{
				throw new Exception("Server is not set");
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
			string profileName = configSection[nameof(AgentSettings.Server)];
			return configSection.GetSection(nameof(AgentSettings.ServerProfiles)).GetChildren().First(x => x[nameof(ServerProfile.Name)] == profileName);
		}
	}
}
