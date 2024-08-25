// Copyright Epic Games, Inc. All Rights Reserved.

using System.ComponentModel;
using EpicGames.Core;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Serilog.Events;

namespace Horde.Agent.Commands.Service
{
	/// <summary>
	/// Runs the agent
	/// </summary>
	[Command("service", "run", "Runs the Horde agent")]
	class RunCommand : Command
	{
		[CommandLine("-LogLevel")]
		[Description("Log verbosity level (use normal MS levels such as debug, warning or information)")]
		public string LogLevelStr { get; set; } = "information";

		readonly DefaultServices _defaultServices;

		/// <summary>
		/// Constructor
		/// </summary>
		public RunCommand(DefaultServices defaultServices)
		{
			_defaultServices = defaultServices;
		}

		/// <summary>
		/// Runs the service indefinitely
		/// </summary>
		/// <returns>Exit code</returns>
		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			if (Enum.TryParse(LogLevelStr, true, out LogEventLevel logEventLevel))
			{
				Logging.LogLevelSwitch.MinimumLevel = logEventLevel;
			}
			else
			{
				Console.WriteLine($"Unable to parse log level: {LogLevelStr}");
				return 0;
			}

			IHostBuilder hostBuilder = Host.CreateDefaultBuilder();

			// Attempt to setup this process as a Windows service. A race condition inside Microsoft.Extensions.Hosting.WindowsServices.WindowsServiceHelpers.IsWindowsService
			// can result in accessing the parent process after it's terminated, so catch any exceptions that it throws.
			try
			{
				hostBuilder = hostBuilder.UseWindowsService();
			}
			catch (InvalidOperationException)
			{
			}

			hostBuilder = hostBuilder
				.ConfigureAppConfiguration(builder =>
				{
					builder.AddConfiguration(_defaultServices.Configuration);
				})
				.ConfigureLogging(builder =>
				{
					// We add our logger through ConfigureServices, inherited from _defaultServices
					builder.ClearProviders();
				})
				.ConfigureServices((hostContext, services) =>
				{
					services.Configure<HostOptions>(options =>
					{
						// Allow the worker service to terminate any before shutting down
						options.ShutdownTimeout = TimeSpan.FromSeconds(30);
					});

					foreach (ServiceDescriptor descriptor in _defaultServices.Descriptors)
					{
						services.Add(descriptor);
					}
				});

			try
			{
				await hostBuilder.Build().RunAsync();
			}
			catch (OperationCanceledException)
			{
				// Need to catch this to prevent it propagating to the awaiter when pressing ctrl-c
			}

			return 0;
		}
	}
}
