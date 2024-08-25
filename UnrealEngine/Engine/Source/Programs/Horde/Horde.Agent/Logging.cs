// Copyright Epic Games, Inc. All Rights Reserved.

using System.Runtime.InteropServices;
using EpicGames.Core;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Logging;
using OpenTracing;
using OpenTracing.Util;
using Serilog;
using Serilog.Core;
using Serilog.Extensions.Logging;
using Serilog.Formatting.Json;
using Serilog.Sinks.SystemConsole.Themes;

namespace Horde.Agent
{
	static class Logging
	{
		static string s_env = "default";

		public static void SetEnv(string newEnv)
		{
			s_env = newEnv;
		}

		public static LoggingLevelSwitch LogLevelSwitch = new LoggingLevelSwitch();

		private class DatadogLogEnricher : ILogEventEnricher
		{
			public void Enrich(Serilog.Events.LogEvent logEvent, ILogEventPropertyFactory propertyFactory)
			{
				logEvent.AddOrUpdateProperty(propertyFactory.CreateProperty("dd.env", s_env));
				logEvent.AddOrUpdateProperty(propertyFactory.CreateProperty("dd.service", "hordeagent"));
				logEvent.AddOrUpdateProperty(propertyFactory.CreateProperty("dd.version", AgentApp.Version));

				ISpan? span = GlobalTracer.Instance?.ActiveSpan;
				if (span != null)
				{
					logEvent.AddPropertyIfAbsent(propertyFactory.CreateProperty("dd.trace_id", span.Context.TraceId));
					logEvent.AddPropertyIfAbsent(propertyFactory.CreateProperty("dd.span_id", span.Context.SpanId));
				}
			}
		}

		public static ILoggerFactory CreateLoggerFactory(IConfiguration configuration)
		{
			Serilog.ILogger logger = CreateSerilogLogger(configuration);
			return new SerilogLoggerFactory(logger, true);
		}

		static Serilog.ILogger CreateSerilogLogger(IConfiguration configuration)
		{
			DirectoryReference.CreateDirectory(AgentApp.DataDir);

			ConsoleTheme theme;
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows) && Environment.OSVersion.Version < new Version(10, 0))
			{
				theme = SystemConsoleTheme.Literate;
			}
			else
			{
				theme = AnsiConsoleTheme.Code;
			}

			return new LoggerConfiguration()
				.WriteTo.Console(outputTemplate: "[{Timestamp:HH:mm:ss} {Level:w3}] {Indent}{Message:l}{NewLine}{Exception}", theme: theme)
				.WriteTo.File(FileReference.Combine(AgentApp.DataDir, "Log-.txt").FullName, fileSizeLimitBytes: 50 * 1024 * 1024, rollingInterval: RollingInterval.Day, rollOnFileSizeLimit: true, retainedFileCountLimit: 10)
				.WriteTo.File(new JsonFormatter(renderMessage: true), FileReference.Combine(AgentApp.DataDir, "Log-.json").FullName, fileSizeLimitBytes: 50 * 1024 * 1024, rollingInterval: RollingInterval.Day, rollOnFileSizeLimit: true, retainedFileCountLimit: 10)
				.ReadFrom.Configuration(configuration)
				.MinimumLevel.ControlledBy(LogLevelSwitch)
				.Enrich.FromLogContext()
				.Enrich.With<DatadogLogEnricher>()
				.CreateLogger();
		}

		public static ILoggerProvider CreateFileLoggerProvider(DirectoryReference baseDir, string name)
		{
			DirectoryReference.CreateDirectory(baseDir);

			Serilog.Core.Logger logger = new LoggerConfiguration()
				.WriteTo.File(FileReference.Combine(baseDir, $"{name}.txt").FullName)
				.WriteTo.File(new JsonFormatter(renderMessage: true), FileReference.Combine(baseDir, $"{name}.json").FullName)
				.Enrich.FromLogContext()
				.CreateLogger();

			return new SerilogLoggerProvider(logger, true);
		}
	}
}
