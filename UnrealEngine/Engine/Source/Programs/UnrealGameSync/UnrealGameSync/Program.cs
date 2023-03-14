// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Perforce;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Sentry;
using Sentry.Infrastructure;
using System;
using System.Collections.Generic;
using System.Data.SqlClient;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Net;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Forms;

namespace UnrealGameSync
{
	static class Program
	{
		public static string GetVersionString()
		{
			AssemblyInformationalVersionAttribute? version = Assembly.GetExecutingAssembly().GetCustomAttribute<AssemblyInformationalVersionAttribute>();
			return version?.InformationalVersion ?? "Unknown";
		}

		public static string? SyncVersion = null;

		public static void CaptureException(Exception exception)
		{
			if (DeploymentSettings.SentryDsn != null)
			{
				SentrySdk.CaptureException(exception);
			}
		}

		[STAThread]
		static void Main(string[] args)
		{
			if (DeploymentSettings.SentryDsn != null)
			{
				SentryOptions sentryOptions = new SentryOptions();
				sentryOptions.Dsn = DeploymentSettings.SentryDsn;
				sentryOptions.StackTraceMode = StackTraceMode.Enhanced;
				sentryOptions.AttachStacktrace = true;
				sentryOptions.TracesSampleRate = 1.0;
				sentryOptions.SendDefaultPii = true;
				sentryOptions.Debug = true;
				sentryOptions.AutoSessionTracking = true;
				sentryOptions.DetectStartupTime = StartupTimeDetectionMode.Best;
				sentryOptions.ReportAssembliesMode = ReportAssembliesMode.InformationalVersion;
				sentryOptions.DiagnosticLogger = new TraceDiagnosticLogger(SentryLevel.Debug);
				SentrySdk.Init(sentryOptions);

				Application.ThreadException += Application_ThreadException_Sentry;
				AppDomain.CurrentDomain.UnhandledException += CurrentDomain_UnhandledException_Sentry;
				TaskScheduler.UnobservedTaskException += Application_UnobservedException_Sentry;
			}

			bool firstInstance;
			using (Mutex instanceMutex = new Mutex(true, "UnrealGameSyncRunning", out firstInstance))
			{
				if (firstInstance)
				{
					Application.EnableVisualStyles();
					Application.SetCompatibleTextRenderingDefault(false);
				}

				using (EventWaitHandle activateEvent = new EventWaitHandle(false, EventResetMode.AutoReset, "ActivateUnrealGameSync"))
				{
					// handle any url passed in, possibly exiting
					if (UriHandler.ProcessCommandLine(args, firstInstance, activateEvent))
					{
						return;
					}

					if (firstInstance)
					{
						GuardedInnerMainAsync(instanceMutex, activateEvent, args);
					}
					else
					{
						activateEvent.Set();
					}
				}
			}
		}

		static void GuardedInnerMainAsync(Mutex instanceMutex, EventWaitHandle activateEvent, string[] args)
		{
			try
			{
				InnerMainAsync(instanceMutex, activateEvent, args).GetAwaiter().GetResult();
			}
			catch (Exception ex)
			{
				CaptureException(ex);
			}
		}

		static async Task InnerMainAsync(Mutex instanceMutex, EventWaitHandle activateEvent, string[] args)
		{
			string? serverAndPort = null;
			string? userName = null;
			string? baseUpdatePath = null;
			bool previewSetting = false;
			GlobalPerforceSettings.ReadGlobalPerforceSettings(ref serverAndPort, ref userName, ref baseUpdatePath, ref previewSetting);

			List<string> remainingArgs = new List<string>(args);

			string? updateSpawn;
			ParseArgument(remainingArgs, "-updatespawn=", out updateSpawn);

			string? updatePath;
			ParseArgument(remainingArgs, "-updatepath=", out updatePath);

			bool restoreState;
			ParseOption(remainingArgs, "-restorestate", out restoreState);

			bool unstable;
			ParseOption(remainingArgs, "-unstable", out unstable);
			bool preview;
			ParseOption(remainingArgs, "-preview", out preview);
			preview |= unstable;

            string? projectFileName;
            ParseArgument(remainingArgs, "-project=", out projectFileName);

			string? uri;
			ParseArgument(remainingArgs, "-uri=", out uri);

			FileReference updateConfigFile = FileReference.Combine(new FileReference(Assembly.GetExecutingAssembly().Location).Directory, "AutoUpdate.ini");
			MergeUpdateSettings(updateConfigFile, ref updatePath, ref updateSpawn);

			// Set the current working directory to the update directory to prevent child-process file handles from disrupting auto-updates
			if (updateSpawn != null)
			{
				if (File.Exists(updateSpawn))
				{
					Directory.SetCurrentDirectory(Path.GetDirectoryName(updateSpawn));
				}
			}

			string syncVersionFile = Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location!)!, "SyncVersion.txt");
			if(File.Exists(syncVersionFile))
			{
				try
				{
					SyncVersion = File.ReadAllText(syncVersionFile).Trim();
				}
				catch(Exception)
				{
					SyncVersion = null;
				}
			}

			DirectoryReference dataFolder = DirectoryReference.Combine(DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.LocalApplicationData)!, "UnrealGameSync");
			DirectoryReference.CreateDirectory(dataFolder);

			// Enable TLS 1.1 and 1.2. TLS 1.0 is now deprecated and not allowed by default in NET Core servers.
			ServicePointManager.SecurityProtocol |= SecurityProtocolType.Tls11 | SecurityProtocolType.Tls12;

			// Create a new logger
			using (ILoggerProvider loggerProvider = Logging.CreateLoggerProvider(FileReference.Combine(dataFolder, "UnrealGameSync.log")))
			{
				ServiceCollection services = new ServiceCollection();
				services.AddLogging(builder => builder.AddProvider(loggerProvider));
				services.AddSingleton<IAsyncDisposer, AsyncDisposer>();

				await using (ServiceProvider serviceProvider = services.BuildServiceProvider())
				{
					ILoggerFactory loggerFactory = serviceProvider.GetRequiredService<ILoggerFactory>();

					ILogger logger = loggerFactory.CreateLogger("Startup");
					logger.LogInformation("Application version: {Version}", Assembly.GetExecutingAssembly().GetName().Version);
					logger.LogInformation("Started at {Time}", DateTime.Now.ToString());

					string sessionId = Guid.NewGuid().ToString();
					logger.LogInformation("SessionId: {SessionId}", sessionId);

					if (serverAndPort == null || userName == null)
					{
						logger.LogInformation("Missing server settings; finding defaults.");
						serverAndPort ??= PerforceSettings.Default.ServerAndPort;
						userName ??= PerforceSettings.Default.UserName;
						GlobalPerforceSettings.SaveGlobalPerforceSettings(serverAndPort, userName, baseUpdatePath, previewSetting);
					}

					ILogger telemetryLogger = loggerProvider.CreateLogger("Telemetry");
					telemetryLogger.LogInformation("Creating telemetry sink for session {SessionId}", sessionId);

					using (ITelemetrySink telemetrySink = DeploymentSettings.CreateTelemetrySink(userName, sessionId, telemetryLogger))
					{
						ITelemetrySink? prevTelemetrySink = Telemetry.ActiveSink;
						try
						{
							Telemetry.ActiveSink = telemetrySink;

							Telemetry.SendEvent("Startup", new { User = Environment.UserName, Machine = Environment.MachineName });

							AppDomain.CurrentDomain.UnhandledException += CurrentDomain_UnhandledException;

							IPerforceSettings defaultSettings = new PerforceSettings(serverAndPort, userName) { PreferNativeClient = true };

							ProtocolHandlerUtils.InstallQuiet(logger);

							using (UpdateMonitor updateMonitor = new UpdateMonitor(defaultSettings, updatePath, serviceProvider))
							{
								using ProgramApplicationContext context = new ProgramApplicationContext(defaultSettings, updateMonitor, DeploymentSettings.ApiUrl, dataFolder, activateEvent, restoreState, updateSpawn, projectFileName, preview, serviceProvider, uri);
								Application.Run(context);

								if (updateMonitor.IsUpdateAvailable && updateSpawn != null)
								{
									instanceMutex.Close();
									bool launchPreview = updateMonitor.RelaunchPreview ?? preview;
									Utility.SpawnProcess(updateSpawn, "-restorestate" + (launchPreview ? " -unstable" : ""));
								}
							}
						}
						catch (Exception ex)
						{
							Telemetry.SendEvent("Crash", new { Exception = ex.ToString() });
							throw;
						}
						finally
						{
							Telemetry.ActiveSink = prevTelemetrySink;
						}
					}
				}
			}
		}

		private static void CurrentDomain_UnhandledException(object sender, UnhandledExceptionEventArgs args)
		{
			Exception? ex = args.ExceptionObject as Exception;
			if(ex != null)
			{
				Telemetry.SendEvent("Crash", new {Exception = ex.ToString()});
			}
		}

		private static void CurrentDomain_UnhandledException_Sentry(object sender, UnhandledExceptionEventArgs args)
		{
			Exception? ex = args.ExceptionObject as Exception;
			if (ex != null)
			{
				SentrySdk.CaptureException(ex);
			}
		}

		private static void Application_ThreadException_Sentry(object sender, ThreadExceptionEventArgs e)
		{
			SentrySdk.CaptureException(e.Exception);

			ThreadExceptionDialog dialog = new ThreadExceptionDialog(e.Exception);
			dialog.ShowDialog();
		}

		private static void Application_UnobservedException_Sentry(object? sender, UnobservedTaskExceptionEventArgs args)
		{
			Exception? innerException = args.Exception?.InnerException;
			if (innerException != null)
			{
				SentrySdk.CaptureException(innerException, s => s.SetTag("Unobserved", "1"));
			}
		}

		static void MergeUpdateSettings(FileReference updateConfigFile, ref string? updatePath, ref string? updateSpawn)
		{
			try
			{
				ConfigFile updateConfig = new ConfigFile();
				if(FileReference.Exists(updateConfigFile))
				{
					updateConfig.Load(updateConfigFile);
				}

				if(updatePath == null)
				{
					updatePath = updateConfig.GetValue("Update.Path", null);
				}
				else
				{
					updateConfig.SetValue("Update.Path", updatePath);
				}

				if(updateSpawn == null)
				{
					updateSpawn = updateConfig.GetValue("Update.Spawn", null);
				}
				else
				{
					updateConfig.SetValue("Update.Spawn", updateSpawn);
				}

				updateConfig.Save(updateConfigFile);
			}
			catch(Exception)
			{
			}
		}

		static bool ParseOption(List<string> remainingArgs, string option, out bool value)
		{
			for(int idx = 0; idx < remainingArgs.Count; idx++)
			{
				if(remainingArgs[idx].Equals(option, StringComparison.InvariantCultureIgnoreCase))
				{
					value = true;
					remainingArgs.RemoveAt(idx);
					return true;
				}
			}

			value = false;
			return false;
		}

		static bool ParseArgument(List<string> remainingArgs, string prefix, [NotNullWhen(true)] out string? value)
		{
			for(int idx = 0; idx < remainingArgs.Count; idx++)
			{
				if(remainingArgs[idx].StartsWith(prefix, StringComparison.InvariantCultureIgnoreCase))
				{
					value = remainingArgs[idx].Substring(prefix.Length);
					remainingArgs.RemoveAt(idx);
					return true;
				}
			}

			value = null;
			return false;
		}

		public static IEnumerable<string> GetPerforcePaths()
		{
			string? pathList = Environment.GetEnvironmentVariable("PATH");
			if (!String.IsNullOrEmpty(pathList))
			{
				foreach (string pathEntry in pathList.Split(Path.PathSeparator))
				{
					string? perforcePath = null;
					try
					{
						string testPerforcePath = Path.Combine(pathEntry, "p4.exe");
						if (File.Exists(testPerforcePath))
						{
							perforcePath = testPerforcePath;
						}
					}
					catch
					{
					}

					if (perforcePath != null)
					{
						yield return perforcePath;
					}
				}
			}
		}

		public static void SpawnP4Vc(string arguments)
		{
			string executable = "p4vc.exe";

			foreach (string perforcePath in GetPerforcePaths())
			{
				string? perforceDir = Path.GetDirectoryName(perforcePath);
				if (perforceDir != null && File.Exists(Path.Combine(perforceDir, "p4vc.bat")) && !File.Exists(Path.Combine(perforceDir, "p4vc.exe")))
				{
					executable = Path.Combine(perforceDir, "p4v.exe");
					arguments = "-p4vc " + arguments;
					break;
				}
			}

			if (!Utility.SpawnHiddenProcess(executable, arguments))
			{
				MessageBox.Show("Unable to spawn p4vc. Check you have P4V installed.");
			}
		}
	}
}
