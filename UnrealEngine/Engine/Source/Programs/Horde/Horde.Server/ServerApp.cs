// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Server.Commands;
using Horde.Server.Utilities;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Win32;
using Serilog;
using Serilog.Configuration;
using Serilog.Exceptions;
using Serilog.Exceptions.Core;
using Serilog.Exceptions.Grpc.Destructurers;
using Serilog.Formatting.Json;
using Serilog.Sinks.SystemConsole.Themes;

namespace Horde.Server
{
	static class LoggerExtensions
	{
		public static LoggerConfiguration Console(this LoggerSinkConfiguration sinkConfig, ServerSettings settings)
		{
			if (settings.LogJsonToStdOut)
			{
				return sinkConfig.Console(new JsonFormatter(renderMessage: true));
			}
			else
			{
				ConsoleTheme theme;
				if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows) && Environment.OSVersion.Version < new Version(10, 0))
				{
					theme = SystemConsoleTheme.Literate;
				}
				else
				{
					theme = AnsiConsoleTheme.Code;
				}
				return sinkConfig.Console(outputTemplate: "[{Timestamp:HH:mm:ss} {Level:w3}] {Indent}{Message:l}{NewLine}{Exception}", theme: theme, restrictedToMinimumLevel: Serilog.Events.LogEventLevel.Debug);
			}
		}

		public static LoggerConfiguration WithHordeConfig(this LoggerConfiguration configuration, ServerSettings settings)
		{
			if (settings.OpenTelemetry.EnableDatadogCompatibility)
			{
				configuration = configuration.Enrich.With<OpenTelemetryDatadogLogEnricher>();
			}

			return configuration;
		}
	}

	class ServerApp
	{
		public static SemVer Version { get; } = GetVersion();

		public static string DeploymentEnvironment { get; } = GetEnvironment();

		public static string SessionId { get; } = Guid.NewGuid().ToString("n");

		public static DirectoryReference AppDir { get; } = GetAppDir();

		public static DirectoryReference DataDir => s_dataDir;

		public static DirectoryReference ConfigDir => s_configDir;

		public static FileReference ServerConfigFile => s_serverConfigFile ?? throw new InvalidOperationException("ServerConfigFile has not been initialized");

		public static Type[] ConfigSchemas = FindSchemaTypes();

		private static DirectoryReference s_dataDir = DirectoryReference.Combine(GetAppDir(), "Data");
		private static DirectoryReference s_configDir = DirectoryReference.Combine(GetAppDir(), "Defaults");
		private static FileReference? s_serverConfigFile;

		static Type[] FindSchemaTypes()
		{
			List<Type> schemaTypes = new List<Type>();
			foreach (Type type in Assembly.GetExecutingAssembly().GetTypes())
			{
				if (type.GetCustomAttribute<JsonSchemaAttribute>() != null)
				{
					schemaTypes.Add(type);
				}
			}
			return schemaTypes.ToArray();
		}

		static SemVer GetVersion()
		{
			FileVersionInfo versionInfo = FileVersionInfo.GetVersionInfo(Assembly.GetExecutingAssembly().Location);
			if (String.IsNullOrEmpty(versionInfo.ProductVersion))
			{
				return SemVer.Parse("0.0.0");
			}
			else
			{
				return SemVer.Parse(versionInfo.ProductVersion);
			}
		}

		public static async Task<int> Main(string[] args)
		{
			CommandLineArguments arguments = new CommandLineArguments(args);

			// Create the base configuration data by just reading from the application directory. We need to check some settings before
			// being able to read user configuration files.
			IConfiguration baseConfig = CreateConfig(false, null);

			ServerSettings baseServerSettings = new ServerSettings();
			Startup.BindServerSettings(baseConfig, baseServerSettings);

			// Set the default data directory
			if (baseServerSettings.DataDir != null)
			{
				s_dataDir = DirectoryReference.Combine(GetAppDir(), baseServerSettings.DataDir);
			}
			else if (baseServerSettings.Installed && RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				DirectoryReference? commonDataDir = DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.CommonApplicationData);
				if (commonDataDir != null)
				{
					s_dataDir = DirectoryReference.Combine(commonDataDir, "Epic", "Horde", "Server");
				}
			}

			// For installed builds, copy default config files to the data dir and use that as the config dir instead
			if (baseServerSettings.Installed)
			{
				await CopyDefaultConfigFilesAsync(s_configDir, s_dataDir, CancellationToken.None);
				s_configDir = s_dataDir;
			}

			// Create the final configuration, including the server.json file
			s_serverConfigFile = FileReference.Combine(s_configDir, "server.json");
			IConfiguration config = CreateConfig(baseServerSettings.Installed, s_serverConfigFile);

			// Bind the complete settings
			ServerSettings serverSettings = new ServerSettings();
			Startup.BindServerSettings(config, serverSettings);

			DirectoryReference logDir = DirectoryReference.Combine(DataDir, "Logs");
			Serilog.Log.Logger = new LoggerConfiguration()
				.WithHordeConfig(serverSettings)
				.Enrich.FromLogContext()
				.Enrich.WithExceptionDetails(new DestructuringOptionsBuilder()
					.WithDefaultDestructurers()
					.WithDestructurers(new[] { new RpcExceptionDestructurer() }))
				.WriteTo.Console(serverSettings)
				.WriteTo.File(Path.Combine(logDir.FullName, "Log.txt"), outputTemplate: "[{Timestamp:HH:mm:ss} {Level:w3}] {Indent}{Message:l}{NewLine}{Exception}", rollingInterval: RollingInterval.Day, rollOnFileSizeLimit: true, fileSizeLimitBytes: 20 * 1024 * 1024, retainedFileCountLimit: 10)
				.WriteTo.File(new JsonFormatter(renderMessage: true), Path.Combine(logDir.FullName, "Log.json"), rollingInterval: RollingInterval.Day, rollOnFileSizeLimit: true, fileSizeLimitBytes: 20 * 1024 * 1024, retainedFileCountLimit: 10)
				.ReadFrom.Configuration(config)
				.CreateLogger();

			ServiceCollection services = new ServiceCollection();
			services.AddCommandsFromAssembly(Assembly.GetExecutingAssembly());
			services.AddLogging(builder => builder.AddSerilog());
			services.AddSingleton<IConfiguration>(config);
			services.AddSingleton<ServerSettings>(serverSettings);
			services.Configure<ServerSettings>(x => Startup.BindServerSettings(config, x));

#pragma warning disable ASP0000 // Do not call 'IServiceCollection.BuildServiceProvider' in 'ConfigureServices'
			await using (ServiceProvider serviceProvider = services.BuildServiceProvider())
			{
				return await CommandHost.RunAsync(arguments, serviceProvider, typeof(ServerCommand));
			}
#pragma warning restore ASP0000 // Do not call 'IServiceCollection.BuildServiceProvider' in 'ConfigureServices'
		}

		// Used by WebApplicationFactory in controller tests. Uses reflection to call this exact function signature.
		public static IHostBuilder CreateHostBuilder(string[] args) => ServerCommand.CreateHostBuilderForTesting(args);

		/// <summary>
		/// Gets the current environment
		/// </summary>
		/// <returns></returns>
		static string GetEnvironment()
		{
			string? environment = Environment.GetEnvironmentVariable("ASPNETCORE_ENVIRONMENT");
			if (String.IsNullOrEmpty(environment))
			{
				environment = "Production";
			}
			return environment;
		}

		/// <summary>
		/// Get the application directory
		/// </summary>
		/// <returns></returns>
		static DirectoryReference GetAppDir()
		{
			return new FileReference(Assembly.GetExecutingAssembly().Location).Directory;
		}

		/// <summary>
		/// Constructs a configuration object for the current environment
		/// </summary>
		/// <returns></returns>
		static IConfiguration CreateConfig(bool readInstalledConfig, FileReference? serverConfigFile)
		{
			IConfigurationBuilder builder = new ConfigurationBuilder();
			if (readInstalledConfig && OperatingSystem.IsWindows())
			{
				builder = builder.Add(new RegistryConfigurationSource(Registry.LocalMachine, "SOFTWARE\\Epic Games\\Horde\\Server", ServerSettings.SectionName));
			}

			builder.SetBasePath(AppDir.FullName)
				.AddJsonFile("appsettings.json", optional: false)
				.AddJsonFile("appsettings.Build.json", optional: true) // specific settings for builds (installer/dockerfile)
				.AddJsonFile($"appsettings.{DeploymentEnvironment}.json", optional: true) // environment variable overrides, also used in k8s setups with Helm
				.AddJsonFile("appsettings.User.json", optional: true);

			if (serverConfigFile != null)
			{
				builder = builder.AddJsonFile(serverConfigFile.FullName, optional: true, reloadOnChange: true);
			}

			return builder.AddEnvironmentVariables().Build();
		}

		static async Task CopyDefaultConfigFilesAsync(DirectoryReference sourceDir, DirectoryReference targetDir, CancellationToken cancellationToken)
		{
			DirectoryReference.CreateDirectory(targetDir);
			foreach (FileReference sourceFile in DirectoryReference.EnumerateFiles(sourceDir))
			{
				if ((sourceFile.HasExtension(".json") || sourceFile.HasExtension(".png")) && !sourceFile.GetFileName().StartsWith("default", StringComparison.OrdinalIgnoreCase))
				{
					FileReference targetFile = FileReference.Combine(targetDir, sourceFile.GetFileName());
					if (!FileReference.Exists(targetFile))
					{
						// Copy the data to the output file. Create a new file to reset permissions.
						using (FileStream targetStream = FileReference.Open(targetFile, FileMode.Create, FileAccess.Write, FileShare.Read))
						{
							using FileStream sourceStream = FileReference.Open(sourceFile, FileMode.Open, FileAccess.Read, FileShare.Read);
							await sourceStream.CopyToAsync(targetStream, cancellationToken);
						}
					}
				}
			}
		}
	}
}
