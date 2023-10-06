// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Net.Sockets;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using Datadog.Trace;
using Datadog.Trace.Configuration;
using Datadog.Trace.OpenTracing;
using EpicGames.Core;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Compute.Buffers;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using Horde.Agent.Execution;
using Horde.Agent.Leases;
using Horde.Agent.Leases.Handlers;
using Horde.Agent.Services;
using Horde.Agent.Utility;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using OpenTracing.Util;
using Polly;

namespace Horde.Agent
{
	using ITracer = OpenTracing.ITracer;

	/// <summary>
	/// Injectable list of existing services
	/// </summary>
	class DefaultServices
	{
		/// <summary>
		/// The base configuration object
		/// </summary>
		public IConfiguration Configuration { get; }

		/// <summary>
		/// List of service descriptors
		/// </summary>
		public IEnumerable<ServiceDescriptor> Descriptors { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public DefaultServices(IConfiguration configuration, IEnumerable<ServiceDescriptor> descriptors)
		{
			Configuration = configuration;
			Descriptors = descriptors;
		}
	}

	/// <summary>
	/// Entry point
	/// </summary>
	public static class Program
	{
		/// <summary>
		/// Name of the http client
		/// </summary>
		public const string HordeServerClientName = "HordeServer";

		/// <summary>
		/// Path to the root application directory
		/// </summary>
		public static DirectoryReference AppDir { get; } = GetAppDir();

		/// <summary>
		/// Path to the default data directory
		/// </summary>
		public static DirectoryReference DataDir { get; } = GetDataDir();

		/// <summary>
		/// The launch arguments
		/// </summary>
		public static IReadOnlyList<string> Args { get; private set; } = null!;

		/// <summary>
		/// The current application version
		/// </summary>
		public static string Version { get; } = GetVersion();

		/// <summary>
		/// Default settings for json serialization
		/// </summary>
		public static JsonSerializerOptions DefaultJsonSerializerOptions { get; } = new JsonSerializerOptions { AllowTrailingCommas = true, PropertyNameCaseInsensitive = true, PropertyNamingPolicy = JsonNamingPolicy.CamelCase, DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull };

		/// <summary>
		/// Entry point
		/// </summary>
		/// <param name="args">Command-line arguments</param>
		/// <returns>Exit code</returns>
		public static async Task<int> Main(string[] args)
		{
			Program.Args = args;

			string? environment = Environment.GetEnvironmentVariable("DOTNET_ENVIRONMENT");
			if (String.IsNullOrEmpty(environment))
			{
				environment = "Production";
			}

			CommandLineArguments arguments = new CommandLineArguments(args);

			Dictionary<string, string> configOverrides = new Dictionary<string, string>();
			if (arguments.TryGetValue("-Server=", out string? serverOverride))
			{
				configOverrides.Add($"{AgentSettings.SectionName}:{nameof(AgentSettings.Server)}", serverOverride);
			}
			if (arguments.TryGetValue("-WorkingDir=", out string? workingDirOverride))
			{
				configOverrides.Add($"{AgentSettings.SectionName}:{nameof(AgentSettings.WorkingDir)}", workingDirOverride);
			}

			IConfiguration configuration = new ConfigurationBuilder()
				.SetBasePath(AppDir.FullName)
				.AddJsonFile("appsettings.json", optional: false)
				.AddJsonFile("appsettings.Build.json", optional: true) // specific settings for builds (installer/dockerfile)
				.AddJsonFile($"appsettings.{environment}.json", optional: true) // environment variable overrides, also used in k8s setups with Helm
				.AddJsonFile("appsettings.User.json", optional: true)
				.AddInMemoryCollection(configOverrides)
				.AddEnvironmentVariables()
				.Build();

			using ILoggerFactory loggerFactory = Logging.CreateLoggerFactory(configuration);

			IServiceCollection services = new ServiceCollection();
			services.AddCommandsFromAssembly(Assembly.GetExecutingAssembly());
			services.AddSingleton(loggerFactory);

			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				// Prioritize agent execution time over any job its running.
				// We've seen file copying starving the agent communication to the Horde server, causing a disconnect.
				// Increasing the process priority is speculative fix to combat this.
				using (Process process = Process.GetCurrentProcess())
				{
					process.PriorityClass = ProcessPriorityClass.High;
				}
			}

			// Add all the default 
			IConfigurationSection configSection = configuration.GetSection(AgentSettings.SectionName);
			services.AddOptions<AgentSettings>().Configure(options => configSection.Bind(options)).ValidateDataAnnotations();

			AgentSettings settings = new AgentSettings();
			configSection.Bind(settings);

			ServerProfile serverProfile = settings.GetCurrentServerProfile();
			ConfigureTracing(serverProfile.Environment, Program.Version);

			Logging.SetEnv(serverProfile.Environment);

			ILogger certificateLogger = loggerFactory.CreateLogger(typeof(CertificateHelper).FullName!);
			services.AddHttpClient(Program.HordeServerClientName, config =>
			{
				config.BaseAddress = serverProfile.Url;
				config.DefaultRequestHeaders.Add("Accept", "application/json");
				config.Timeout = TimeSpan.FromSeconds(300); // Need to make sure this doesn't cancel any long running gRPC streaming calls (eg. session update)
			})
			.ConfigurePrimaryHttpMessageHandler(() =>
			{
				HttpClientHandler handler = new HttpClientHandler();
				handler.ServerCertificateCustomValidationCallback += (sender, cert, chain, errors) => CertificateHelper.CertificateValidationCallBack(certificateLogger, sender, cert, chain, errors, serverProfile);
				return handler;
			})
			.AddTransientHttpErrorPolicy(builder =>
			{
				return builder.WaitAndRetryAsync(new[] { TimeSpan.FromSeconds(1), TimeSpan.FromSeconds(5), TimeSpan.FromSeconds(10) });
			});

			services.AddHttpClient(HttpStorageClient.HttpClientName)
				.AddTransientHttpErrorPolicy(builder =>
				{
					return builder.WaitAndRetryAsync(new[] { TimeSpan.FromSeconds(1), TimeSpan.FromSeconds(5), TimeSpan.FromSeconds(10) });
				});

			services.AddSingleton<GrpcService>();
			services.AddSingleton<TelemetryService>();
			services.AddHostedService(sp => sp.GetRequiredService<TelemetryService>());

			services.AddSingleton<IJobExecutorFactory, PerforceExecutorFactory>();
			services.AddSingleton<IJobExecutorFactory, WorkspaceExecutorFactory>();
			services.AddSingleton<IJobExecutorFactory, LocalExecutorFactory>();
			services.AddSingleton<IJobExecutorFactory, TestExecutorFactory>();
			
			services.AddSingleton<IWorkspaceMaterializerFactory, WorkspaceMaterializerFactory>();

			services.AddSingleton<JobHandler>();
			services.AddSingleton<StatusService>();
			services.AddHostedService<StatusService>(sp => sp.GetRequiredService<StatusService>());

			services.AddSingleton<LeaseHandler, ComputeHandler>();
			services.AddSingleton<LeaseHandler, ConformHandler>();
			services.AddSingleton<LeaseHandler, JobHandler>(x => x.GetRequiredService<JobHandler>());
			services.AddSingleton<LeaseHandler, RestartHandler>();
			services.AddSingleton<LeaseHandler, ShutdownHandler>();
			services.AddSingleton<LeaseHandler, UpgradeHandler>();

			services.AddSingleton<CapabilitiesService>();
			services.AddSingleton<ISessionFactory, SessionFactory>();
			services.AddSingleton<IServerLoggerFactory, ServerLoggerFactory>();
			services.AddSingleton<IServerStorageFactory, HttpServerStorageFactory>();
			services.AddSingleton<WorkerService>();
			services.AddHostedService(sp => sp.GetRequiredService<WorkerService>());

			services.AddSingleton<IStorageClientFactory, StorageClientFactory>();

			services.AddSingleton<ComputeListenerService>();
			services.AddHostedService(sp => sp.GetRequiredService<ComputeListenerService>());

			services.AddMemoryCache();

			// Allow commands to augment the service collection for their own DI service providers
			services.AddSingleton<DefaultServices>(x => new DefaultServices(configuration, services));

			// Execute all the commands
			IServiceProvider serviceProvider = services.BuildServiceProvider();
			return await CommandHost.RunAsync(arguments, serviceProvider, typeof(Horde.Agent.Modes.Service.RunCommand));
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


		/// <summary>
		/// Gets the version of the current assembly
		/// </summary>
		/// <returns></returns>
		static string GetVersion()
		{
			try
			{
				return FileVersionInfo.GetVersionInfo(Assembly.GetExecutingAssembly().Location).ProductVersion!;
			}
			catch
			{
				return "unknown";
			}
		}

		/// <summary>
		/// Gets the application directory
		/// </summary>
		/// <returns></returns>
		static DirectoryReference GetAppDir()
		{
			return new DirectoryReference(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location)!);
		}

		/// <summary>
		/// Gets the default data directory
		/// </summary>
		/// <returns></returns>
		static DirectoryReference GetDataDir()
		{
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				DirectoryReference? programDataDir = DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.CommonApplicationData);
				if (programDataDir != null)
				{
					return DirectoryReference.Combine(programDataDir, "HordeAgent");
				}
			}
			return GetAppDir();
		}
	}
}
