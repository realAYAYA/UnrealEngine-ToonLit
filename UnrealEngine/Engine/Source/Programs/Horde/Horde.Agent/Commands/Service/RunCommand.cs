// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Threading.Tasks;
using Datadog.Trace;
using Datadog.Trace.Configuration;
using Datadog.Trace.OpenTracing;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using Horde.Agent.Services;
using Horde.Agent.Utility;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using OpenTracing;
using OpenTracing.Util;
using Polly;

namespace Horde.Agent.Modes.Service
{
	using ITracer = OpenTracing.ITracer;

	/// <summary>
	/// 
	/// </summary>
	[Command("Service", "Run", "Runs the service in listen mode")]
	class RunCommand : Command
	{
		/// <summary>
		/// Override for the server to use
		/// </summary>
		[CommandLine("-Server=")]
		string? Server { get; set; } = null;

		/// <summary>
		/// Override the working directory
		/// </summary>
		[CommandLine("-WorkingDir=")]
		string? WorkingDir { get; set; } = null;

		/// <summary>
		/// Runs the service indefinitely
		/// </summary>
		/// <returns>Exit code</returns>
		public override async Task<int> ExecuteAsync(ILogger logger)
		{
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
					Dictionary<string, string> overrides = new Dictionary<string, string>();
					if (Server != null)
					{
						overrides.Add($"{AgentSettings.SectionName}:{nameof(AgentSettings.Server)}", Server);
					}
					if (WorkingDir != null)
					{
						overrides.Add($"{AgentSettings.SectionName}:{nameof(AgentSettings.WorkingDir)}", WorkingDir);
					}
					builder.AddInMemoryCollection(overrides);
				})
				.ConfigureLogging(builder =>
				{
					builder.ClearProviders();
					builder.AddProvider(new Logging.HordeLoggerProvider());
					builder.AddFilter<Logging.HordeLoggerProvider>(null, LogLevel.Trace);
				})
				.ConfigureServices((hostContext, services) =>
				{
					services.Configure<HostOptions>(options =>
					{
						// Allow the worker service to terminate any before shutting down
						options.ShutdownTimeout = TimeSpan.FromSeconds(30);
					});

					IConfigurationSection configSection = hostContext.Configuration.GetSection(AgentSettings.SectionName);
					services.AddOptions<AgentSettings>().Configure(options => configSection.Bind(options)).ValidateDataAnnotations();

					AgentSettings settings = new AgentSettings();
					configSection.Bind(settings);

					ServerProfile serverProfile = settings.GetCurrentServerProfile();
					ConfigureTracing(serverProfile.Environment, Program.Version);

					Logging.SetEnv(serverProfile.Environment);

					services.AddHttpClient(Program.HordeServerClientName, config =>
					{
						config.BaseAddress = serverProfile.Url;
						config.DefaultRequestHeaders.Add("Accept", "application/json");
						config.Timeout = TimeSpan.FromSeconds(300); // Need to make sure this doesn't cancel any long running gRPC streaming calls (eg. session update)
					})
					.ConfigurePrimaryHttpMessageHandler(() =>
					{
						HttpClientHandler handler = new HttpClientHandler();
						handler.ServerCertificateCustomValidationCallback += (sender, cert, chain, errors) => CertificateHelper.CertificateValidationCallBack(logger, sender, cert, chain, errors, serverProfile);
						return handler;
					})
					.AddTransientHttpErrorPolicy(builder =>
					{
						return builder.WaitAndRetryAsync(new[] { TimeSpan.FromSeconds(1), TimeSpan.FromSeconds(5), TimeSpan.FromSeconds(10) });
					});

					services.AddHordeStorage(settings => configSection.GetCurrentServerProfile().GetSection(nameof(serverProfile.Storage)).Bind(settings));

					services.AddSingleton<GrpcService>();
					services.AddHostedService<WorkerService>();
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

		static void ConfigureTracing(string environment, string version)
		{
			TracerSettings settings = TracerSettings.FromDefaultSources();
			settings.Environment = environment;
			settings.ServiceName = "hordeagent";
			settings.ServiceVersion = version;
			settings.LogsInjectionEnabled = true;

			Tracer.Configure(settings);

			ITracer openTracer = OpenTracingTracerFactory.WrapTracer(Tracer.Instance);
			GlobalTracer.Register(openTracer);
		}
	}
}
