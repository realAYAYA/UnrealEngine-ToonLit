// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using System.Reflection;
using System.Text;
using EpicGames.Core;
using Horde.Agent.Utility;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Commands.Service
{
	/// <summary>
	/// Installs the agent as a Windows service
	/// </summary>
	[Command("service", "install", "Installs the agent as a Windows service")]
	class InstallCommand : Command
	{
		/// <summary>
		/// Name of the service
		/// </summary>
		public const string ServiceName = "HordeAgent";

		[CommandLine("-UserName=")]
		[Description("Specifies the username for the service to run under")]
		public string? UserName { get; set; } = null;

		[CommandLine("-Password=")]
		[Description("Password for the service account")]
		public string? Password { get; set; } = null;

		[CommandLine("-Server=")]
		[Description("The server profile to use")]
		public string? Server { get; set; } = null;

		[CommandLine("-DotNetExecutable=")]
		[Description("Path to dotnet executable (dotnet.exe on Windows). When left empty, the value of \"dotnet\" will be used.")]
		public string DotNetExecutable { get; set; } = "dotnet";

		[CommandLine("-Start=")]
		[Description("Whether to start the service after installation (true/false)")]
		public string? Start { get; set; } = "true";

		/// <summary>
		/// Installs the service
		/// </summary>
		/// <param name="logger">Logger to use</param>
		/// <returns>Exit code</returns>
		public override Task<int> ExecuteAsync(ILogger logger)
		{
			if (!RuntimePlatform.IsWindows)
			{
				logger.LogError("This command requires Windows");
				return Task.FromResult(1);
			}

			using (WindowsServiceManager serviceManager = new WindowsServiceManager())
			{
				using (WindowsService service = serviceManager.Open(ServiceName))
				{
					if (service.IsValid)
					{
						logger.LogInformation("Stopping existing service...");
						service.Stop();

						WindowsServiceStatus status = service.WaitForStatusChange(WindowsServiceStatus.Stopping, TimeSpan.FromSeconds(30.0));
						if (status != WindowsServiceStatus.Stopped)
						{
							logger.LogError("Unable to stop service (status = {Status})", status);
							return Task.FromResult(1);
						}

						logger.LogInformation("Deleting service");
						service.Delete();
					}
				}

				logger.LogInformation("Registering {ServiceName} service", ServiceName);

				StringBuilder commandLine = new();
				if (AgentApp.IsSelfContained)
				{
					if (Environment.ProcessPath == null)
					{
						logger.LogError("Unable to detect current process path");
						return Task.FromResult(1);
					}
					commandLine.Append($"\"{Environment.ProcessPath}\" service run");
				}
				else
				{
#pragma warning disable IL3000 // Avoid accessing Assembly file path when publishing as a single file
					commandLine.AppendFormat("{0} \"{1}\" service run", DotNetExecutable, Assembly.GetEntryAssembly()!.Location);
#pragma warning restore IL3000 // Avoid accessing Assembly file path when publishing as a single file					
				}

				if (Server != null)
				{
					commandLine.Append($" -server={Server}");
				}

				using (WindowsService service = serviceManager.Create(ServiceName, "Horde Agent", commandLine.ToString(), UserName, Password))
				{
					service.SetDescription("Allows this machine to participate in a Horde farm.");

					if (Start != null && Start.Equals("true", StringComparison.OrdinalIgnoreCase))
					{
						logger.LogInformation("Starting...");
						service.Start();

						WindowsServiceStatus status = service.WaitForStatusChange(WindowsServiceStatus.Starting, TimeSpan.FromSeconds(30.0));
						if (status != WindowsServiceStatus.Running)
						{
							logger.LogError("Unable to start service (status = {Status})", status);
							return Task.FromResult(1);
						}
					}

					logger.LogInformation("Done.");
				}
			}
			return Task.FromResult(0);
		}
	}
}
