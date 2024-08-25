// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Text.Json;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using OpenTracing.Util;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	static class UnrealBuildTool
	{
		/// <summary>
		/// Save the application startup time. This can be used as the timestamp for build makefiles, to determine a base time after which any
		/// modifications should invalidate it.
		/// </summary>
		public static DateTime StartTimeUtc { get; } = DateTime.UtcNow;

		/// <summary>
		/// The environment at boot time.
		/// </summary>
		public static System.Collections.IDictionary? InitialEnvironment;

		/// <summary>
		/// Whether we're running with an installed project
		/// </summary>
		private static bool? bIsProjectInstalled;

		/// <summary>
		/// If we are running with an installed project, specifies the path to it
		/// </summary>
		static FileReference? InstalledProjectFile;

		/// <summary>
		/// The full name of the Engine/Source directory
		/// </summary>
		[Obsolete("Replace with Unreal.EngineSourceDirectory")]
		public static readonly DirectoryReference EngineSourceDirectory = Unreal.EngineSourceDirectory;

		/// <summary>
		/// Cached copy of the source root directory that was used to compile the installed engine
		/// Used to remap source code paths when debugging.
		/// </summary>
		static DirectoryReference? CachedOriginalCompilationRootDirectory;

		/// <summary>
		/// Writable engine directory. Uses the user's settings folder for installed builds.
		/// </summary>
		[Obsolete("Replace with Unreal.WritableEngineDirectory")]
		public static DirectoryReference WritableEngineDirectory = Unreal.WritableEngineDirectory;

		/// <summary>
		/// The engine programs directory
		/// </summary>
		[Obsolete("Replace with Unreal.EngineProgramSavedDirectory")]
		public static DirectoryReference EngineProgramSavedDirectory = Unreal.EngineProgramSavedDirectory;

		/// <summary>
		/// The original root directory that was used to compile the installed engine
		/// Used to remap source code paths when debugging.
		/// </summary>
		public static DirectoryReference OriginalCompilationRootDirectory
		{
			get
			{
				if (CachedOriginalCompilationRootDirectory == null)
				{
					if (Unreal.IsEngineInstalled())
					{
						// Load Engine\Intermediate\Build\BuildRules\*RulesManifest.json
						DirectoryReference BuildRules = DirectoryReference.Combine(Unreal.EngineDirectory, "Intermediate", "Build", "BuildRules");
						FileReference? RulesManifest = DirectoryReference.EnumerateFiles(BuildRules, "*RulesManifest.json").FirstOrDefault();
						if (RulesManifest != null)
						{
							JsonObject Manifest = JsonObject.Read(RulesManifest);
							if (Manifest.TryGetStringArrayField("SourceFiles", out string[]? SourceFiles))
							{
								FileReference? SourceFile = FileReference.FromString(SourceFiles.FirstOrDefault());
								if (SourceFile != null && !SourceFile.IsUnderDirectory(Unreal.EngineDirectory))
								{
									// Walk up parent directory until Engine is found
									DirectoryReference? Directory = SourceFile.Directory;
									while (Directory != null && !Directory.IsRootDirectory())
									{
										if (Directory.GetDirectoryName() == "Engine" && Directory.ParentDirectory != null)
										{
											CachedOriginalCompilationRootDirectory = Directory.ParentDirectory;
											break;
										}

										Directory = Directory.ParentDirectory;
									}
								}
							}
						}
					}

					if (CachedOriginalCompilationRootDirectory == null)
					{
						CachedOriginalCompilationRootDirectory = Unreal.RootDirectory;
					}
				}
				return CachedOriginalCompilationRootDirectory;
			}
		}

		/// <summary>
		/// The Remote Ini directory.  This should always be valid when compiling using a remote server.
		/// </summary>
		static string? RemoteIniPath = null;

		/// <summary>
		/// Returns true if UnrealBuildTool is running using an installed project (ie. a mod kit)
		/// </summary>
		/// <returns>True if running using an installed project</returns>
		public static bool IsProjectInstalled()
		{
			if (!bIsProjectInstalled.HasValue)
			{
				FileReference InstalledProjectLocationFile = FileReference.Combine(Unreal.RootDirectory, "Engine", "Build", "InstalledProjectBuild.txt");
				if (FileReference.Exists(InstalledProjectLocationFile))
				{
					InstalledProjectFile = FileReference.Combine(Unreal.RootDirectory, File.ReadAllText(InstalledProjectLocationFile.FullName).Trim());
					bIsProjectInstalled = true;
				}
				else
				{
					InstalledProjectFile = null;
					bIsProjectInstalled = false;
				}
			}
			return bIsProjectInstalled.Value;
		}

		/// <summary>
		/// Gets the installed project file
		/// </summary>
		/// <returns>Location of the installed project file</returns>
		public static FileReference? GetInstalledProjectFile()
		{
			if (IsProjectInstalled())
			{
				return InstalledProjectFile;
			}
			else
			{
				return null;
			}
		}

		/// <summary>
		/// Checks whether the given file is under an installed directory, and should not be overridden
		/// </summary>
		/// <param name="File">File to test</param>
		/// <returns>True if the file is part of the installed distribution, false otherwise</returns>
		public static bool IsFileInstalled(FileReference File)
		{
			if (Unreal.IsEngineInstalled() && File.IsUnderDirectory(Unreal.EngineDirectory))
			{
				return true;
			}
			if (IsProjectInstalled() && File.IsUnderDirectory(InstalledProjectFile!.Directory))
			{
				return true;
			}
			return false;
		}

		/// <summary>
		/// Gets the absolute path to the UBT assembly.
		/// </summary>
		/// <returns>A string containing the path to the UBT assembly.</returns>
		[Obsolete("Deprecated in UE5.1 - use UnrealBuildTool.DotnetPath Unreal.UnrealBuildToolDllPath")]
		public static FileReference GetUBTPath() => Unreal.UnrealBuildToolPath;

		/// <summary>
		/// The Unreal remote tool ini directory.  This should be valid if compiling using a remote server
		/// </summary>
		/// <returns>The directory path</returns>
		public static string? GetRemoteIniPath()
		{
			return RemoteIniPath;
		}

		public static void SetRemoteIniPath(string Path)
		{
			RemoteIniPath = Path;
		}

		/// <summary>
		/// Global options for UBT (any modes)
		/// </summary>
		class GlobalOptions
		{
			/// <summary>
			/// User asked for help
			/// </summary>
			[CommandLine(Prefix = "-Help", Description = "Display this help.")]
			[CommandLine(Prefix = "-h")]
			[CommandLine(Prefix = "--help")]
			public bool bGetHelp = false;

			/// <summary>
			/// The amount of detail to write to the log
			/// </summary>
			[CommandLine(Prefix = "-Verbose", Value = "Verbose", Description = "Increase output verbosity")]
			[CommandLine(Prefix = "-VeryVerbose", Value = "VeryVerbose", Description = "Increase output verbosity more")]
			public LogEventType LogOutputLevel = LogEventType.Log;

			/// <summary>
			/// Specifies the path to a log file to write. Note that the default mode (eg. building, generating project files) will create a log file by default if this not specified.
			/// </summary>
			[CommandLine(Prefix = "-Log", Description = "Specify a log file location instead of the default Engine/Programs/UnrealBuildTool/Log.txt")]
			public FileReference? LogFileName = null;

			/// <summary>
			/// Log all attempts to write to the specified file
			/// </summary>
			[CommandLine(Prefix = "-TraceWrites", Description = "Trace writes requested to the specified file")]
			public FileReference? TraceWrites = null;

			/// <summary>
			/// Whether to include timestamps in the log
			/// </summary>
			[CommandLine(Prefix = "-Timestamps", Description = "Include timestamps in the log")]
			public bool bLogTimestamps = false;

			/// <summary>
			/// Whether to format messages in MsBuild format
			/// </summary>
			[CommandLine(Prefix = "-FromMsBuild", Description = "Format messages for msbuild")]
			public bool bLogFromMsBuild = false;

			/// <summary>
			/// Whether or not to suppress warnings of missing SDKs from warnings to LogEventType.Log in UEBuildPlatformSDK.cs 
			/// </summary>
			[CommandLine(Prefix = "-SuppressSDKWarnings", Description = "Missing SDKs error verbosity level will be reduced from warning to log")]
			public bool bShouldSuppressSDKWarnings = false;

			/// <summary>
			/// Whether to write progress markup in a format that can be parsed by other programs
			/// </summary>
			[CommandLine(Prefix = "-Progress", Description = "Write progress messages in a format that can be parsed by other programs")]
			public bool bWriteProgressMarkup = false;

			/// <summary>
			/// Whether to ignore the mutex
			/// </summary>
			[CommandLine(Prefix = "-NoMutex", Description = "Allow more than one instance of the program to run at once")]
			public bool bNoMutex = false;

			/// <summary>
			/// Whether to wait for the mutex rather than aborting immediately
			/// </summary>
			[CommandLine(Prefix = "-WaitMutex", Description = "Wait for another instance to finish and then start, rather than aborting immediately")]
			public bool bWaitMutex = false;

			/// <summary>
			/// </summary>
			[CommandLine(Prefix = "-RemoteIni", Description = "Remote tool ini directory")]
			public string RemoteIni = "";

			/// <summary>
			/// The mode to execute
			/// </summary>
			[CommandLine("-Mode=")] // description handling is special-cased in PrintUsage()

			[CommandLine("-Clean", Value = "Clean", Description = "Clean build products. Equivalent to -Mode=Clean")]

			[CommandLine("-ProjectFiles", Value = "GenerateProjectFiles", Description = "Generate project files based on IDE preference. Equivalent to -Mode=GenerateProjectFiles")]
			[CommandLine("-ProjectFileFormat=", Value = "GenerateProjectFiles", Description = "Generate project files in specified format. May be used multiple times.")]
			[CommandLine("-Makefile", Value = "GenerateProjectFiles", Description = "Generate Linux Makefile")]
			[CommandLine("-CMakefile", Value = "GenerateProjectFiles", Description = "Generate project files for CMake")]
			[CommandLine("-QMakefile", Value = "GenerateProjectFiles", Description = "Generate project files for QMake")]
			[CommandLine("-KDevelopfile", Value = "GenerateProjectFiles", Description = "Generate project files for KDevelop")]
			[CommandLine("-CodeliteFiles", Value = "GenerateProjectFiles", Description = "Generate project files for Codelite")]
			[CommandLine("-XCodeProjectFiles", Value = "GenerateProjectFiles", Description = "Generate project files for XCode")]
			[CommandLine("-EddieProjectFiles", Value = "GenerateProjectFiles", Description = "Generate project files for Eddie")]
			[CommandLine("-VSCode", Value = "GenerateProjectFiles", Description = "Generate project files for Visual Studio Code")]
			[CommandLine("-VSMac", Value = "GenerateProjectFiles", Description = "Generate project files for Visual Studio Mac")]
			[CommandLine("-CLion", Value = "GenerateProjectFiles", Description = "Generate project files for CLion")]
			[CommandLine("-Rider", Value = "GenerateProjectFiles", Description = "Generate project files for Rider")]
#if __VPROJECT_AVAILABLE__
			[CommandLine("-VProject", Value = "GenerateProjectFiles")]
#endif
			public string? Mode = null;

			// The following Log settings exists in this location because, at the time of writing, EpicGames.Core does
			// not have access to XmlConfigFileAttribute.

			/// <summary>
			/// Whether to backup an existing log file, rather than overwriting it.
			/// </summary>
			[XmlConfigFile(Category = "Log")]
			public bool bBackupLogFiles = Log.BackupLogFiles;

			/// <summary>
			/// The number of log file backups to preserve. Older backups will be deleted.
			/// </summary>
			[XmlConfigFile(Category = "Log")]
			public int LogFileBackupCount = Log.LogFileBackupCount;

			/// <summary>
			/// If set TMP\TEMP will be overidden to this directory, each process will create a unique subdirectory in this folder.
			/// </summary>
			[XmlConfigFile(Category = "BuildConfiguration")]
			public string? TempDirectory = null;

			/// <summary>
			/// If set the application temp directory will be deleted on exit, only when running with a single instance mutex.
			/// </summary>
			[XmlConfigFile(Category = "BuildConfiguration")]
			public bool bDeleteTempDirectory = false;

			/// <summary>
			/// Initialize the options with the given command line arguments
			/// </summary>
			/// <param name="Arguments"></param>
			public GlobalOptions(CommandLineArguments Arguments)
			{
				Arguments.ApplyTo(this);
				if (!String.IsNullOrEmpty(RemoteIni))
				{
					UnrealBuildTool.SetRemoteIniPath(RemoteIni);
				}
			}
		}

		/// <summary>
		/// Get all the valid Modes
		/// </summary>
		/// <returns></returns>
		private static Dictionary<string, Type> GetModes()
		{
			Dictionary<string, Type> ModeNameToType = new Dictionary<string, Type>(StringComparer.OrdinalIgnoreCase);
			foreach (Type Type in Assembly.GetExecutingAssembly().GetTypes())
			{
				if (Type.IsClass && !Type.IsAbstract && Type.IsSubclassOf(typeof(ToolMode)))
				{
					ToolModeAttribute? Attribute = Type.GetCustomAttribute<ToolModeAttribute>();
					if (Attribute == null)
					{
						throw new BuildException("Class '{0}' should have a ToolModeAttribute", Type.Name);
					}
					ModeNameToType.Add(Attribute.Name, Type);
				}
			}
			return ModeNameToType;
		}
		public static readonly Dictionary<string, Type> ModeNameToType = GetModes();

		/// <summary>
		/// Print (incomplete) usage information
		/// </summary>
		private static void PrintUsage()
		{
			Console.WriteLine("Global options:");
			int LongestPrefix = 0;
			foreach (FieldInfo Info in typeof(GlobalOptions).GetFields())
			{
				foreach (CommandLineAttribute Att in Info.GetCustomAttributes<CommandLineAttribute>())
				{
					if (Att.Prefix != null && Att.Description != null)
					{
						LongestPrefix = Att.Prefix.Length > LongestPrefix ? Att.Prefix.Length : LongestPrefix;
					}
				}
			}

			foreach (FieldInfo Info in typeof(GlobalOptions).GetFields())
			{
				foreach (CommandLineAttribute Att in Info.GetCustomAttributes<CommandLineAttribute>())
				{
					if (Att.Prefix != null && Att.Description != null)
					{
						Console.WriteLine($"  {Att.Prefix.PadRight(LongestPrefix)} :  {Att.Description}");
					}

					// special case for Mode
					if (String.Equals(Att.Prefix, "-Mode="))
					{
						Console.WriteLine($"  {Att.Prefix!.PadRight(LongestPrefix)} :  Select tool mode. One of the following (default tool mode is \"Build\"):");
						string Indent = "".PadRight(LongestPrefix + 8);
						string Line = Indent;
						IOrderedEnumerable<string> SortedModeNames = ModeNameToType.Keys.ToList().OrderBy(Name => Name);
						foreach (string ModeName in SortedModeNames.SkipLast(1))
						{
							Line += $"{ModeName}, ";
							if (Line.Length > 110)
							{
								Console.WriteLine(Line);
								Line = Indent;
							}
						}
						Line += SortedModeNames.Last();
						Console.WriteLine(Line);
					}
				}
			}
		}

		/// <summary>
		/// Main entry point. Parses any global options and initializes the logging system, then invokes the appropriate command.
		/// NB: That the entry point is deliberately NOT async, since we have a single-instance mutex that cannot be disposed from a different thread.
		/// </summary>
		/// <param name="ArgumentsArray">Command line arguments</param>
		/// <returns>Zero on success, non-zero on error</returns>
		private static int Main(string[] ArgumentsArray)
		{
			FileReference? RunFile = null;
			DirectoryReference? TempDirectory = null;
			SingleInstanceMutex? Mutex = null;
			JsonTracer? Tracer = null;

			ILogger Logger = Log.Logger;

			// When running RunUBT.sh on a Mac we need to install a Ctrl-C handler, or hitting Ctrl-C from a terminal
			// or from cancelling a build within Xcode, can leave a dotnet process in a zombie state. 
			// By putting this in, the Ctrl-C may not be handled immediately, but it shouldn't leave a blocking zombie process
			if (OperatingSystem.IsMacOS())
			{
				Console.CancelKeyPress += delegate
				{
					Console.WriteLine("UnrealBuildTool: Ctrl-C pressed. Exiting...");

					// While the Ctrl-C handler fixes most instances of a zombie process, we still need to 
					// force an exit from the process to handle _all_ cases.  Ctrl-C should not be a regular event! 
					// Note: this could be a dotnet (6.0.302) on macOS issue.  Recheck with next release if this is still required.
					Environment.Exit(-1);
				};
			}

			try
			{
				// Start capturing performance info
				Timeline.Start();
				Tracer = JsonTracer.TryRegisterAsGlobalTracer();

				// Parse the command line arguments
				CommandLineArguments Arguments = new CommandLineArguments(ArgumentsArray);

				// Parse the global options
				GlobalOptions Options = new GlobalOptions(Arguments);

				if (
					// Print usage if there are zero arguments provided
					ArgumentsArray.Length == 0

					// Print usage if the user asks for help
					|| Options.bGetHelp
					)
				{
					PrintUsage();
					return Options.bGetHelp ? 0 : 1;
				}

				// Configure the log system
				Log.OutputLevel = Options.LogOutputLevel;
				Log.IncludeTimestamps = Options.bLogTimestamps;
				Log.IncludeProgramNameWithSeverityPrefix = Options.bLogFromMsBuild;

				// Reducing SDK warning events in the log to LogEventType.Log
				if (Options.bShouldSuppressSDKWarnings)
				{
					UEBuildPlatformSDK.bSuppressSDKWarnings = true;
				}

				// Always start capturing logs as early as possible to later copy to a log file if the ToolMode desires it (we have to start capturing before we get the ToolModeOptions below)
				StartupTraceListener StartupTrace = new StartupTraceListener();
				Log.AddTraceListener(StartupTrace);

				if (Options.TraceWrites != null)
				{
					Logger.LogInformation("All attempts to write to \"{TraceWrites}\" via WriteFileIfChanged() will be logged", Options.TraceWrites);
					Utils.WriteFileIfChangedTrace = Options.TraceWrites;
				}

				// Configure the progress writer
				ProgressWriter.bWriteMarkup = Options.bWriteProgressMarkup;

				// Ensure we can resolve any external assemblies that are not in the same folder as our assembly.
				AssemblyUtils.InstallAssemblyResolver(Path.GetDirectoryName(Assembly.GetEntryAssembly()!.GetOriginalLocation())!);

				// Add the application directory to PATH
				DirectoryReference.AddDirectoryToPath(Unreal.UnrealBuildToolDllPath.Directory);

				// Change the working directory to be the Engine/Source folder. We are likely running from Engine/Binaries/DotNET
				// This is critical to be done early so any code that relies on the current directory being Engine/Source will work.
				DirectoryReference.CreateDirectory(Unreal.EngineSourceDirectory);
				DirectoryReference.SetCurrentDirectory(Unreal.EngineSourceDirectory);

				// Register encodings from Net FW as this is required when using Ionic as we do in multiple toolchains
				Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);

				// Get the type of the mode to execute, using a fast-path for the build mode.
				Type? ModeType = typeof(BuildMode);
				if (Options.Mode != null)
				{
					// Try to get the correct mode
					if (!ModeNameToType.TryGetValue(Options.Mode, out ModeType))
					{
						List<string> ModuleNameList = ModeNameToType.Keys.ToList();
						ModuleNameList.Sort(StringComparer.OrdinalIgnoreCase);
						Logger.LogError("No mode named '{Name}'. Available modes are:\n  {ModeList}", Options.Mode, String.Join("\n  ", ModuleNameList));
						return 1;
					}
				}

				// Get the options for which systems have to be initialized for this mode
				ToolModeOptions ModeOptions = ModeType.GetCustomAttribute<ToolModeAttribute>()!.Options;

				// if we don't care about the trace listener, toss it now
				if ((ModeOptions & ToolModeOptions.UseStartupTraceListener) == 0)
				{
					Log.RemoveTraceListener(StartupTrace);
				}

				// Start prefetching the contents of the engine folder
				if ((ModeOptions & ToolModeOptions.StartPrefetchingEngine) != 0)
				{
					using (GlobalTracer.Instance.BuildSpan("FileMetadataPrefetch.QueueEngineDirectory()").StartActive())
					{
						FileMetadataPrefetch.QueueEngineDirectory();
					}
				}

				// Read the XML configuration files
				if ((ModeOptions & ToolModeOptions.XmlConfig) != 0)
				{
					using (GlobalTracer.Instance.BuildSpan("XmlConfig.ReadConfigFiles()").StartActive())
					{
						string XmlConfigMutexName = SingleInstanceMutex.GetUniqueMutexForPath("UnrealBuildTool_Mutex_XmlConfig", Assembly.GetExecutingAssembly().Location);
						using (SingleInstanceMutex XmlConfigMutex = new SingleInstanceMutex(XmlConfigMutexName, true))
						{
							FileReference? XmlConfigCache = Arguments.GetFileReferenceOrDefault("-XmlConfigCache=", null);
							XmlConfig.ReadConfigFiles(XmlConfigCache, null, Logger);
						}
					}

					XmlConfig.ApplyTo(Options);
				}

				Log.BackupLogFiles = Options.bBackupLogFiles;
				Log.LogFileBackupCount = Options.LogFileBackupCount;

				// Add the log writer if requested. When building a target, we'll create the writer for the default log file later.
				if (Options.LogFileName != null)
				{
					Log.AddFileWriter("LogTraceListener", Options.LogFileName);
				}

				// Create a UbtRun file
				try
				{
					DirectoryReference RunsDir = DirectoryReference.Combine(Unreal.EngineDirectory, "Intermediate", "UbtRuns");
					Directory.CreateDirectory(RunsDir.FullName);
					string ModuleFileName = Process.GetCurrentProcess().MainModule?.FileName ?? "";
					if (!String.IsNullOrEmpty(ModuleFileName))
					{
						ModuleFileName = Path.GetFullPath(ModuleFileName);
					}
					FileReference RunFileTemp = FileReference.Combine(RunsDir, $"{Process.GetCurrentProcess().Id}_{ContentHash.MD5(Encoding.UTF8.GetBytes(ModuleFileName.ToUpperInvariant()))}");
					File.WriteAllLines(RunFileTemp.FullName, new string[] { ModuleFileName });
					RunFile = RunFileTemp;
				}
				catch
				{
				}

				// Override the temp directory
				try
				{
					// If the temp directory is already overridden from a parent process, do not override again
					if (String.IsNullOrEmpty(Environment.GetEnvironmentVariable("UnrealBuildTool_TMP")))
					{
						DirectoryReference OverrideTempDirectory = new DirectoryReference(Path.Combine(Path.GetTempPath(), "UnrealBuildTool"));
						if (Options.TempDirectory != null)
						{
							if (Directory.Exists(Options.TempDirectory))
							{
								OverrideTempDirectory = new DirectoryReference(Options.TempDirectory);
								if (OverrideTempDirectory.GetDirectoryName() != "UnrealBuildTool")
								{
									OverrideTempDirectory = DirectoryReference.Combine(OverrideTempDirectory, "UnrealBuildTool");
								}
							}
							else
							{
								Logger.LogWarning("Warning: TempDirectory override '{Override}' does not exist, using '{Temp}'", Options.TempDirectory, OverrideTempDirectory.FullName);
							}
						}

						OverrideTempDirectory = DirectoryReference.Combine(OverrideTempDirectory, ContentHash.MD5(Encoding.UTF8.GetBytes(Unreal.UnrealBuildToolDllPath.FullName)).ToString().Substring(0, 8));
						DirectoryReference.CreateDirectory(OverrideTempDirectory);

						Logger.LogDebug("Setting temp directory to '{Path}'", OverrideTempDirectory);
						Environment.SetEnvironmentVariable("UnrealBuildTool_TMP", OverrideTempDirectory.FullName);
						Environment.SetEnvironmentVariable("TMP", OverrideTempDirectory.FullName);
						Environment.SetEnvironmentVariable("TEMP", OverrideTempDirectory.FullName);

						// Deleting the directory is only safe in single instance mode, and only if requested
						if ((ModeOptions & ToolModeOptions.SingleInstance) != 0 && !Options.bNoMutex && Options.bDeleteTempDirectory)
						{
							Logger.LogDebug("Temp directory '{Path}' will be deleted on exit", OverrideTempDirectory);
							TempDirectory = OverrideTempDirectory;
						}
					}
				}
				catch
				{
				}

				// Acquire a lock for this branch
				if ((ModeOptions & ToolModeOptions.SingleInstance) != 0 && !Options.bNoMutex)
				{
					using (GlobalTracer.Instance.BuildSpan("SingleInstanceMutex.Acquire()").StartActive())
					{
						string MutexName = SingleInstanceMutex.GetUniqueMutexForPath("UnrealBuildTool_Mutex", Assembly.GetExecutingAssembly().Location);
						Mutex = new SingleInstanceMutex(MutexName, Options.bWaitMutex);
					}
				}

				// Register all the build platforms
				if ((ModeOptions & ToolModeOptions.BuildPlatforms) != 0)
				{
					using (GlobalTracer.Instance.BuildSpan("UEBuildPlatform.RegisterPlatforms()").StartActive())
					{
						UEBuildPlatform.RegisterPlatforms(false, false, ModeType, ArgumentsArray, Logger);
					}
				}
				if ((ModeOptions & ToolModeOptions.BuildPlatformsHostOnly) != 0)
				{
					using (GlobalTracer.Instance.BuildSpan("UEBuildPlatform.RegisterPlatforms()").StartActive())
					{
						UEBuildPlatform.RegisterPlatforms(false, true, ModeType, ArgumentsArray, Logger);
					}
				}
				if ((ModeOptions & ToolModeOptions.BuildPlatformsForValidation) != 0)
				{
					using (GlobalTracer.Instance.BuildSpan("UEBuildPlatform.RegisterPlatforms()").StartActive())
					{
						UEBuildPlatform.RegisterPlatforms(true, false, ModeType, ArgumentsArray, Logger);
					}
				}

				// Create the appropriate handler
				ToolMode Mode = (ToolMode)Activator.CreateInstance(ModeType)!;

				// Execute the mode
				int Result;
				try
				{
					Result = Mode.ExecuteAsync(Arguments, Logger).GetAwaiter().GetResult();
				}
				catch (AggregateException AggEx) when (AggEx.InnerExceptions.Count == 1 && AggEx.InnerExceptions.FirstOrDefault() != null)
				{
					throw AggEx.InnerExceptions.First();
				}
				finally
				{
					if ((ModeOptions & ToolModeOptions.ShowExecutionTime) != 0)
					{
						Logger.LogInformation("Total execution time: {Time:0.00} seconds", Timeline.Elapsed.TotalSeconds);
					}
				}

				return Result;
			}
			catch (CompilationResultException Ex)
			{
				// Used to return a propagate a specific exit code after an error has occurred.
				Ex.LogException(Logger);
				return (int)Ex.Result;
			}
			catch (BuildLogEventException Ex)
			{
				// BuildExceptions should have nicely formatted messages.
				Ex.LogException(Logger);
				return (int)CompilationResult.OtherCompilationError;
			}
			catch (JsonException Ex)
			{
				FileReference source = new FileReference(Ex.Source ?? "unknown");
				LogValue FileValue = LogValue.SourceFile(source, source.GetFileName());
				Logger.LogError(KnownLogEvents.Compiler, "{File}({Line}): error:{Message}", FileValue, Ex.LineNumber ?? 0, ExceptionUtils.FormatException(Ex));
				Logger.LogDebug(KnownLogEvents.Compiler, "{File}({Line}): error:{Message}", FileValue, Ex.LineNumber ?? 0, ExceptionUtils.FormatExceptionDetails(Ex));
				return (int)CompilationResult.OtherCompilationError;
			}
			catch (BuildException Ex)
			{
				// BuildExceptions should have nicely formatted messages.
				Ex.LogException(Logger);
				return (int)CompilationResult.OtherCompilationError;
			}
			catch (Exception Ex)
			{
				// Unhandled exception.
				Logger.LogError(Ex, "Unhandled exception: {Ex}", ExceptionUtils.FormatException(Ex));
				Logger.LogDebug(Ex, "Unhandled exception: {Ex}", ExceptionUtils.FormatExceptionDetails(Ex));
				return (int)CompilationResult.OtherCompilationError;
			}
			finally
			{
				// Cancel the prefetcher
				using (GlobalTracer.Instance.BuildSpan("FileMetadataPrefetch.Stop()").StartActive())
				{
					try
					{
						FileMetadataPrefetch.Stop();
					}
					catch
					{
					}
				}

				// Uncomment this to output a file that contains all files that UBT has scanned.
				// Useful when investigating why UBT takes time.
				//DirectoryItem.WriteDebugFileWithAllEnumeratedFiles(@"c:\temp\AllFiles.txt");

				Utils.LogWriteFileIfChangedActivity(Logger);

				// Print out all the performance info
				Timeline.Print(TimeSpan.FromMilliseconds(20.0), LogLevel.Debug, Logger);

				// Make sure we flush the logs however we exit
				Trace.Close();

				// Write any trace logs
				if (Tracer != null)
				{
					Tracer.Flush();
				}

				// Delete the ubt run file
				if (RunFile != null)
				{
					try
					{
						File.Delete(RunFile.FullName);
					}
					catch
					{
					}
				}

				// Remove the the temp subdirectory. TempDirectory will only be set if running in single instance mode when Options.DeleteTempDirectory is enabled
				if (TempDirectory != null)
				{
					try
					{
						DirectoryReference.Delete(TempDirectory, true);
					}
					catch
					{
					}
				}

				// Dispose of the mutex. Must be done last to ensure that another process does not startup and start trying to write to the same log file.
				if (Mutex != null)
				{
					Mutex.Dispose();
				}
			}
		}
	}
}

