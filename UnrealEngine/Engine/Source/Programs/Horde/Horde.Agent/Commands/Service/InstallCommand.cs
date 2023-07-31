// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Agent.Utility;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Commands.Service
{
	/// <summary>
	/// Installs the agent as a service
	/// </summary>
	[Command("Service", "Install", "Installs the agent as a service")]
	class InstallCommand : Command
	{
		/// <summary>
		/// Name of the service
		/// </summary>
		public const string ServiceName = "HordeAgent";

		/// <summary>
		/// Specifies the username for the service to run under
		/// </summary>
		[CommandLine("-UserName=")]
		public string? UserName { get; set; } = null;

		/// <summary>
		/// Password for the username
		/// </summary>
		[CommandLine("-Password=")]
		public string? Password { get; set; } = null;

		/// <summary>
		/// The server profile to use
		/// </summary>
		[CommandLine("-Server=")]
		public string? Server { get; set; } = null;
		
		/// <summary>
		/// Path to dotnet executable (dotnet.exe on Windows)
		/// When left empty, the value of "dotnet" will be used.
		/// </summary>
		[CommandLine("-DotNetExecutable=")]
		public string DotNetExecutable { get; set; } = "dotnet";

		/// <summary>
		/// Runs the service indefinitely
		/// </summary>
		/// <param name="logger">Logger to use</param>
		/// <returns>Exit code</returns>
		public override Task<int> ExecuteAsync(ILogger logger)
		{
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

				StringBuilder commandLine = new StringBuilder();
				commandLine.AppendFormat("{0} \"{1}\" service run", DotNetExecutable, Assembly.GetEntryAssembly()!.Location);
				if(Server != null)
				{
					commandLine.Append($" -server={Server}");
				}

				using (WindowsService service = serviceManager.Create(ServiceName, "Horde Agent", commandLine.ToString(), UserName, Password))
				{
					service.SetDescription("Allows this machine to participate in a Horde farm.");

					logger.LogInformation("Starting...");
					service.Start();

					WindowsServiceStatus status = service.WaitForStatusChange(WindowsServiceStatus.Starting, TimeSpan.FromSeconds(30.0));
					if (status != WindowsServiceStatus.Running)
					{
						logger.LogError("Unable to start service (status = {Status})", status);
						return Task.FromResult(1);
					}

					logger.LogInformation("Done.");
				}
			}
			return Task.FromResult(0);
		}
	}
}
