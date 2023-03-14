// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Runtime.InteropServices;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using OpenTracing;
using OpenTracing.Util;
using Serilog;
using Serilog.Core;
using Serilog.Events;
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

		private static readonly Lazy<Serilog.ILogger> s_logger = new Lazy<Serilog.ILogger>(CreateSerilogLogger, true);
		
		public static LoggingLevelSwitch LogLevelSwitch =  new LoggingLevelSwitch();

		private class DatadogLogEnricher : ILogEventEnricher
		{
			public void Enrich(Serilog.Events.LogEvent logEvent, ILogEventPropertyFactory propertyFactory)
			{
				logEvent.AddOrUpdateProperty(propertyFactory.CreateProperty("dd.env", s_env));
				logEvent.AddOrUpdateProperty(propertyFactory.CreateProperty("dd.service", "hordeagent"));
				logEvent.AddOrUpdateProperty(propertyFactory.CreateProperty("dd.version", Program.Version));

				ISpan? span = GlobalTracer.Instance?.ActiveSpan;
				if (span != null)
				{
					logEvent.AddPropertyIfAbsent(propertyFactory.CreateProperty("dd.trace_id", span.Context.TraceId));
					logEvent.AddPropertyIfAbsent(propertyFactory.CreateProperty("dd.span_id", span.Context.SpanId));
				}
			}
		}
		
		public sealed class HordeLoggerProvider : ILoggerProvider
		{
			private readonly SerilogLoggerProvider _inner;

			public HordeLoggerProvider()
			{
				_inner = new SerilogLoggerProvider(s_logger.Value);
			}

			public Microsoft.Extensions.Logging.ILogger CreateLogger(string categoryName)
			{
				Microsoft.Extensions.Logging.ILogger logger = _inner.CreateLogger(categoryName);
				logger = new DefaultLoggerIndentHandler(logger);
				return logger;
			}

			public void Dispose()
			{
				_inner.Dispose();
			}
		}

		static Serilog.ILogger CreateSerilogLogger()
		{
			DirectoryReference.CreateDirectory(Program.DataDir);

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
				.MinimumLevel.Debug()
				.MinimumLevel.Override("Microsoft", LogEventLevel.Information)
				.MinimumLevel.Override("System.Net.Http.HttpClient", LogEventLevel.Warning)
				.MinimumLevel.Override("Microsoft.AspNetCore.Routing.EndpointMiddleware", LogEventLevel.Warning)
				.MinimumLevel.Override("Microsoft.AspNetCore.Authorization.DefaultAuthorizationService", LogEventLevel.Warning)
				.MinimumLevel.Override("HordeServer.Authentication.HordeJwtBearerHandler", LogEventLevel.Warning)
				.MinimumLevel.Override("HordeServer.Authentication.OktaHandler", LogEventLevel.Warning)
				.MinimumLevel.Override("Microsoft.AspNetCore.Hosting.Diagnostics", LogEventLevel.Warning)
				.MinimumLevel.Override("Microsoft.AspNetCore.Mvc.Infrastructure.ControllerActionInvoker", LogEventLevel.Warning)
				.MinimumLevel.Override("Serilog.AspNetCore.RequestLoggingMiddleware", LogEventLevel.Warning)
				.MinimumLevel.ControlledBy(LogLevelSwitch)
				.Enrich.FromLogContext()
				.Enrich.With<DatadogLogEnricher>()
				.WriteTo.Console(outputTemplate: "[{Timestamp:HH:mm:ss} {Level:w3}] {Indent}{Message:l}{NewLine}{Exception}", theme: theme)
				.WriteTo.File(FileReference.Combine(Program.DataDir, "Log-.txt").FullName, fileSizeLimitBytes: 50 * 1024 * 1024, rollingInterval: RollingInterval.Day, rollOnFileSizeLimit: true, retainedFileCountLimit: 10)
				.WriteTo.File(new JsonFormatter(renderMessage: true), FileReference.Combine(Program.DataDir, "Log-.json").FullName, fileSizeLimitBytes: 50 * 1024 * 1024, rollingInterval: RollingInterval.Day, rollOnFileSizeLimit: true, retainedFileCountLimit: 10)
				.CreateLogger();
		}
	}
}
