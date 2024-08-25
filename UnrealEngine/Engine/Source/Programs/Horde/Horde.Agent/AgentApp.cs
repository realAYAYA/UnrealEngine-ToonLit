// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Text.Json.Serialization;
using Datadog.Trace;
using Datadog.Trace.Configuration;
using Datadog.Trace.OpenTracing;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Clients;
using Horde.Agent.Execution;
using Horde.Agent.Leases;
using Horde.Agent.Leases.Handlers;
using Horde.Agent.Services;
using Horde.Agent.Utility;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using Microsoft.Win32;
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
	public static class AgentApp
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
		public static DirectoryReference DataDir { get; private set; } = DirectoryReference.Combine(AppDir, "Data");

		/// <summary>
		/// The launch arguments
		/// </summary>
		public static IReadOnlyList<string> Args { get; private set; } = null!;

#pragma warning disable IL3000 // Avoid accessing Assembly file path when publishing as a single file
		/// <summary>
		/// Whether agent is packaged as a self-contained package where the .NET runtime is included.
		/// </summary>
		public static bool IsSelfContained => String.IsNullOrEmpty(Assembly.GetExecutingAssembly().Location);
#pragma warning restore IL3000 // Avoid accessing Assembly file path when publishing as a single file		

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
			AgentApp.Args = args;

			CommandLineArguments arguments = new CommandLineArguments(args);

			Dictionary<string, string?> configOverrides = new Dictionary<string, string?>();
			if (arguments.TryGetValue("-Server=", out string? serverOverride))
			{
				configOverrides.Add($"{AgentSettings.SectionName}:{nameof(AgentSettings.Server)}", serverOverride);
			}
			if (arguments.TryGetValue("-WorkingDir=", out string? workingDirOverride))
			{
				configOverrides.Add($"{AgentSettings.SectionName}:{nameof(AgentSettings.WorkingDir)}", workingDirOverride);
			}

			// Create the base configuration data by just reading from the application directory. We need to check some settings before
			// being able to read user configuration files.
			IConfiguration configuration = CreateConfig(false, null, configOverrides);
			AgentSettings settings = BindSettings(configuration);

			if (settings.Installed)
			{
				if (OperatingSystem.IsWindows())
				{
					DirectoryReference? commonAppDataDir = DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.CommonApplicationData);
					if (commonAppDataDir != null)
					{
						DataDir = DirectoryReference.Combine(commonAppDataDir, "Epic", "Horde", "Agent");
						await CopyDefaultConfigFilesAsync(DataDir);
					}
				}

				FileReference agentConfigFile = FileReference.Combine(DataDir, "agent.json");
				configuration = CreateConfig(true, agentConfigFile, configOverrides);
				settings = BindSettings(configuration);
			}

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

			ServerProfile serverProfile = settings.GetCurrentServerProfile();
			ConfigureTracing(serverProfile.Environment, AgentApp.Version);

			Logging.SetEnv(serverProfile.Environment);

			ILogger certificateLogger = loggerFactory.CreateLogger(typeof(CertificateHelper).FullName!);
			services.AddHttpClient(AgentApp.HordeServerClientName, config =>
			{
				config.BaseAddress = serverProfile.Url;
				config.DefaultRequestHeaders.Add("Accept", "application/json");
				config.Timeout = TimeSpan.FromSeconds(300); // Need to make sure this doesn't cancel any long running gRPC streaming calls (eg. session update)
			})
			.ConfigurePrimaryHttpMessageHandler(() =>
			{
				HttpClientHandler handler = new HttpClientHandler();
#pragma warning disable MA0039
				handler.ServerCertificateCustomValidationCallback += (sender, cert, chain, errors) => CertificateHelper.CertificateValidationCallBack(certificateLogger, sender, cert, chain, errors, serverProfile);
#pragma warning restore MA0039
				return handler;
			})
			.AddTransientHttpErrorPolicy(builder =>
			{
				return builder.WaitAndRetryAsync(new[] { TimeSpan.FromSeconds(1), TimeSpan.FromSeconds(5), TimeSpan.FromSeconds(10) });
			});

			services.Configure<HordeOptions>(options => options.AllowAuthPrompt = false);

			services.AddHordeHttpClient(client => client.BaseAddress = serverProfile.Url)
				.ConfigurePrimaryHttpMessageHandler(() => new SocketsHttpHandler
				{
					MaxConnectionsPerServer = 16,
					PooledConnectionIdleTimeout = TimeSpan.FromMinutes(15),
				});

			services.AddHttpClient(AwsInstanceLifecycleService.HttpClientName)
				.AddTransientHttpErrorPolicy(builder =>
				{
					return builder.WaitAndRetryAsync(new[] { TimeSpan.FromSeconds(1), TimeSpan.FromSeconds(5), TimeSpan.FromSeconds(5), TimeSpan.FromSeconds(5) });
				});
			services.AddSingleton<AwsInstanceLifecycleService>();
			if (settings.EnableAwsEc2Support)
			{
				services.AddHostedService(sp => sp.GetRequiredService<AwsInstanceLifecycleService>());
			}

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
			services.AddSingleton<WorkerService>();
			services.AddSingleton<LeaseLoggerFactory>();
			services.AddHostedService(sp => sp.GetRequiredService<WorkerService>());

			services.AddSingleton<BundleCache>();
			services.AddSingleton<StorageBackendCache>(CreateStorageBackendCache);
			services.AddSingleton<HttpStorageBackendFactory>();
			services.AddSingleton<HttpStorageClientFactory>();

			services.AddSingleton<ComputeListenerService>();
			services.AddHostedService(sp => sp.GetRequiredService<ComputeListenerService>());

			services.AddMemoryCache();

			// Allow commands to augment the service collection for their own DI service providers
			services.AddSingleton<DefaultServices>(x => new DefaultServices(configuration, services));

			// Execute all the commands
			await using ServiceProvider serviceProvider = services.BuildServiceProvider();
			return await CommandHost.RunAsync(arguments, serviceProvider, typeof(Commands.Service.RunCommand));
		}

		static IConfiguration CreateConfig(bool readInstalledConfig, FileReference? agentConfigFile, Dictionary<string, string?> configOverrides)
		{
			string? environment = Environment.GetEnvironmentVariable("DOTNET_ENVIRONMENT");
			if (String.IsNullOrEmpty(environment))
			{
				environment = "Production";
			}

			IConfigurationBuilder builder = new ConfigurationBuilder();
			if (readInstalledConfig && OperatingSystem.IsWindows())
			{
				builder = builder.Add(new RegistryConfigurationSource(Registry.LocalMachine, "SOFTWARE\\Epic Games\\Horde\\Agent", AgentSettings.SectionName));
			}

			builder.SetBasePath(AppDir.FullName)
				.AddJsonFile("appsettings.json", optional: false)
				.AddJsonFile("appsettings.Build.json", optional: true) // specific settings for builds (installer/dockerfile)
				.AddJsonFile($"appsettings.{environment}.json", optional: true) // environment variable overrides, also used in k8s setups with Helm
				.AddJsonFile("appsettings.User.json", optional: true);

			if (agentConfigFile != null)
			{
				builder = builder.AddJsonFile(agentConfigFile.FullName, optional: true, reloadOnChange: true);
			}

			return builder
				.AddInMemoryCollection(configOverrides)
				.AddEnvironmentVariables()
				.Build();
		}

		static AgentSettings BindSettings(IConfiguration configuration)
		{
			AgentSettings settings = new AgentSettings();
			configuration.GetSection(AgentSettings.SectionName).Bind(settings);
			return settings;
		}

		static async Task CopyDefaultConfigFilesAsync(DirectoryReference configDir)
		{
			DirectoryReference.CreateDirectory(configDir);

			DirectoryReference defaultsDir = DirectoryReference.Combine(AppDir, "Defaults");
			if (DirectoryReference.Exists(defaultsDir))
			{
				foreach (FileReference sourceFile in DirectoryReference.EnumerateFiles(defaultsDir, "*.json"))
				{
					FileReference targetFile = FileReference.Combine(configDir, sourceFile.GetFileName());
					if (!FileReference.Exists(targetFile))
					{
						using FileStream targetStream = FileReference.Open(targetFile, FileMode.Create, FileAccess.Write, FileShare.Read);
						using FileStream sourceStream = FileReference.Open(sourceFile, FileMode.Open, FileAccess.Read, FileShare.Read);
						await sourceStream.CopyToAsync(targetStream);
					}
				}
			}
		}

		static StorageBackendCache CreateStorageBackendCache(IServiceProvider serviceProvider)
		{
			AgentSettings settings = serviceProvider.GetRequiredService<IOptions<AgentSettings>>().Value;
			DirectoryReference cacheDir = DirectoryReference.Combine(settings.WorkingDir, "Saved", "Bundles");
			return new StorageBackendCache(cacheDir, settings.BundleCacheSize * 1024 * 1024, serviceProvider.GetRequiredService<ILogger<StorageBackendCache>>());
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
		[SuppressMessage("SingleFile", "IL3000:Avoid accessing Assembly file path when publishing as a single file", Justification = "Has fallback handling")]
		static string GetVersion()
		{
			try
			{
				string? assemblyPath = Assembly.GetExecutingAssembly().Location;
				if (String.IsNullOrEmpty(assemblyPath))
				{
					// It's possible the current assembly is packaged as self-contained, try resolving path another way
					assemblyPath = Process.GetCurrentProcess().MainModule?.FileName;
				}

				if (assemblyPath != null)
				{
					string? version = FileVersionInfo.GetVersionInfo(assemblyPath).ProductVersion;
					if (version != null)
					{
						return version;
					}
				}
			}
			catch
			{
				// Ignore
			}

			return "unknown";
		}

		/// <summary>
		/// Gets the application directory
		/// </summary>
		/// <returns></returns>
		[SuppressMessage("SingleFile", "IL3000:Avoid accessing Assembly file path when publishing as a single file", Justification = "Has fallback handling")]
		static DirectoryReference GetAppDir()
		{
			string? directoryName = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
			if (!String.IsNullOrEmpty(directoryName))
			{
				return new DirectoryReference(directoryName);
			}

			// When C# project is packaged as a single file, GetExecutingAssembly above does not work
			return DirectoryReference.FromFile(new FileReference(Environment.ProcessPath!));
		}
	}
}
