// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Build.Commands;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using OpenTracing;
using OpenTracing.Propagation;
using OpenTracing.Util;
using Serilog;
using Serilog.Configuration;
using Serilog.Core;
using Serilog.Events;
using Serilog.Formatting.Json;
using Serilog.Sinks.SystemConsole.Themes;

namespace Horde.Build
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
				return sinkConfig.Console(outputTemplate: "[{Timestamp:HH:mm:ss} {Level:w3}] {Indent}{Message:l}{NewLine}{Exception}", theme: theme, restrictedToMinimumLevel: LogEventLevel.Debug);
			}
		}

		public static LoggerConfiguration WithHordeConfig(this LoggerConfiguration configuration, ServerSettings settings)
		{
			if (settings.WithDatadog)
			{
				configuration = configuration.Enrich.With<DatadogLogEnricher>();
			}
			return configuration;
		}
	}

	class DatadogLogEnricher : ILogEventEnricher
	{
		public void Enrich(Serilog.Events.LogEvent logEvent, ILogEventPropertyFactory propertyFactory)
		{
			ISpan? span = GlobalTracer.Instance?.ActiveSpan;
			if (span != null)
			{
				logEvent.AddPropertyIfAbsent(propertyFactory.CreateProperty("dd.trace_id", span.Context.TraceId));
				logEvent.AddPropertyIfAbsent(propertyFactory.CreateProperty("dd.span_id", span.Context.SpanId));
			}
		}
	}

	class TestTracer : ITracer
	{
		readonly ITracer _inner;

		public TestTracer(ITracer inner)
		{
			_inner = inner;
		}

		public IScopeManager ScopeManager => _inner.ScopeManager;

		public ISpan ActiveSpan => _inner.ActiveSpan;

		public ISpanBuilder BuildSpan(string operationName)
		{
			Serilog.Log.Debug("Creating span {Name}", operationName);
			return _inner.BuildSpan(operationName);
		}

		public ISpanContext Extract<TCarrier>(IFormat<TCarrier> format, TCarrier carrier)
		{
			return _inner.Extract<TCarrier>(format, carrier);
		}

		public void Inject<TCarrier>(ISpanContext spanContext, IFormat<TCarrier> format, TCarrier carrier)
		{
			_inner.Inject<TCarrier>(spanContext, format, carrier);
		}
	}

	class Program
	{
		public static SemVer Version => _version;

		public static DirectoryReference AppDir { get; } = GetAppDir();

		public static DirectoryReference DataDir { get; } = GetDefaultDataDir();

		public static FileReference UserConfigFile { get; } = FileReference.Combine(GetDefaultDataDir(), "Horde.json");

		public static Type[] ConfigSchemas = FindSchemaTypes();

		static SemVer _version;

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

		public static async Task<int> Main(string[] args)
		{
			FileVersionInfo versionInfo = FileVersionInfo.GetVersionInfo(Assembly.GetExecutingAssembly().Location);
			if (String.IsNullOrEmpty(versionInfo.ProductVersion))
			{
				_version = SemVer.Parse("0.0.0");
			}
			else
			{
				_version = SemVer.Parse(versionInfo.ProductVersion);
			}

			CommandLineArguments arguments = new CommandLineArguments(args);

			IConfiguration config = new ConfigurationBuilder()
				.SetBasePath(AppDir.FullName)
				.AddJsonFile("appsettings.json", optional: false) 
				.AddJsonFile("appsettings.Build.json", optional: true) // specific settings for builds (installer/dockerfile)
				.AddJsonFile($"appsettings.{Environment.GetEnvironmentVariable("ASPNETCORE_ENVIRONMENT")}.json", optional: true) // environment variable overrides, also used in k8s setups with Helm
				.AddJsonFile("appsettings.User.json", optional: true)
				.AddJsonFile(UserConfigFile.FullName, optional: true, reloadOnChange: true)
				.AddEnvironmentVariables()
				.Build();

			ServerSettings hordeSettings = new ServerSettings();
			config.GetSection("Horde").Bind(hordeSettings);

			InitializeDefaults(hordeSettings);

			DirectoryReference logDir = AppDir;
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				logDir = DirectoryReference.Combine(DataDir);
			}

			Serilog.Log.Logger = new LoggerConfiguration()
				.WithHordeConfig(hordeSettings)
				.Enrich.FromLogContext()
				.WriteTo.Console(hordeSettings)
				.WriteTo.File(Path.Combine(logDir.FullName, "Log.txt"), outputTemplate: "[{Timestamp:HH:mm:ss} {Level:w3}] {Indent}{Message:l}{NewLine}{Exception} [{SourceContext}]", rollingInterval: RollingInterval.Day, rollOnFileSizeLimit: true, fileSizeLimitBytes: 20 * 1024 * 1024, retainedFileCountLimit: 10)
				.WriteTo.File(new JsonFormatter(renderMessage: true), Path.Combine(logDir.FullName, "Log.json"), rollingInterval: RollingInterval.Day, rollOnFileSizeLimit: true, fileSizeLimitBytes: 20 * 1024 * 1024, retainedFileCountLimit: 10)
				.ReadFrom.Configuration(config)
				.CreateLogger();

			Serilog.Log.Logger.Information("Server version: {Version}", Version);

			if (hordeSettings.WithDatadog)
			{
				GlobalTracer.Register(Datadog.Trace.OpenTracing.OpenTracingTracerFactory.WrapTracer(Datadog.Trace.Tracer.Instance));
				Serilog.Log.Logger.Information("Enabling datadog tracing (OpenTrace)");
			}

			IServiceCollection services = new ServiceCollection();
			services.AddCommandsFromAssembly(Assembly.GetExecutingAssembly());
			services.AddLogging(builder => builder.AddSerilog());
			services.AddSingleton<IConfiguration>(config);
			services.AddSingleton<ServerSettings>(hordeSettings);

#pragma warning disable ASP0000 // Do not call 'IServiceCollection.BuildServiceProvider' in 'ConfigureServices'
			IServiceProvider serviceProvider = services.BuildServiceProvider();
			return await CommandHost.RunAsync(arguments, serviceProvider, typeof(ServerCommand));
#pragma warning restore ASP0000 // Do not call 'IServiceCollection.BuildServiceProvider' in 'ConfigureServices'
		}

		// Used by WebApplicationFactory in controller tests. Uses reflection to call this exact function signature.
		public static IHostBuilder CreateHostBuilder(string[] args) => ServerCommand.CreateHostBuilderForTesting(args);

		/// <summary>
		/// Get the application directory
		/// </summary>
		/// <returns></returns>
		static DirectoryReference GetAppDir()
		{
			return new FileReference(Assembly.GetExecutingAssembly().Location).Directory;
		}

		/// <summary>
		/// Gets the default directory for storing application data
		/// </summary>
		/// <returns>The default data directory</returns>
		static DirectoryReference GetDefaultDataDir()
		{
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				DirectoryReference? dir = DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.CommonApplicationData);
				if (dir != null)
				{
					return DirectoryReference.Combine(dir, "HordeServer");
				}
			}
			return DirectoryReference.Combine(GetAppDir(), "Data");
		}

		/// <summary>
		/// Handles bootstrapping of defaults for local servers, which can't be generated during build/installation process (or are better handled here where they can be updated)
		/// This stuff will change as we get settings into database and could be considered discovery for installer/dockerfile builds 
		/// </summary>		
		static void InitializeDefaults(ServerSettings settings)
		{			
			if (settings.SingleInstance)
			{
				FileReference globalConfig = FileReference.Combine(Program.DataDir, "Config/globals.json");

				if (!FileReference.Exists(globalConfig))
				{
					DirectoryReference.CreateDirectory(globalConfig.Directory);
					FileReference.WriteAllText(globalConfig, "{}");
				}

				FileReference privateCertFile = FileReference.Combine(Program.DataDir, "Agent/ServerToAgent.pfx");
				string privateCertFileJsonPath = privateCertFile.ToString().Replace("\\", "/", StringComparison.Ordinal);

				if (!FileReference.Exists(UserConfigFile))
				{
					// create new user configuration
					DirectoryReference.CreateDirectory(UserConfigFile.Directory);
					FileReference.WriteAllText(UserConfigFile, $"{{\"Horde\": {{ \"ConfigPath\" : \"{globalConfig.ToString().Replace("\\", "/", StringComparison.Ordinal)}\", \"ServerPrivateCert\" : \"{privateCertFileJsonPath}\", \"HttpPort\": 8080}}}}");
				}

				// make sure the cert exists
				if (!FileReference.Exists(privateCertFile))
				{
					string dnsName = System.Net.Dns.GetHostName();
					Serilog.Log.Logger.Information("Creating certificate for {DnsName}", dnsName);

					byte[] privateCertData = CertificateUtils.CreateSelfSignedCert(dnsName, "Horde Server");
					
					Serilog.Log.Logger.Information("Writing private cert: {PrivateCert}", privateCertFile.FullName);

					if (!DirectoryReference.Exists(privateCertFile.Directory))
					{
						DirectoryReference.CreateDirectory(privateCertFile.Directory);
					}

					FileReference.WriteAllBytes(privateCertFile, privateCertData);
				}

				// note: this isn't great, though we need it early in server startup, and this is only hit on first server boot where the grpc cert isn't generated/set 
				if (settings.ServerPrivateCert == null)
				{
					settings.ServerPrivateCert = privateCertFile.ToString();
				}				
			}
		}
	}
}
