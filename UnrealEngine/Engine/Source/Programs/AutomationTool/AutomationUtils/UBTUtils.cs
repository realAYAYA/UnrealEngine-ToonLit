// Copyright Epic Games, Inc. All Rights Reserved.
using EpicGames.Core;
using System;
using System.Linq;
using UnrealBuildBase;
using UnrealBuildTool;

namespace AutomationTool
{

	public partial class CommandUtils
	{
		/// <summary>
		/// Runs UBT with the specified command line. Automatically creates a logfile. When 
		/// no LogName is specified, the executable name is used as logfile base name.
		/// </summary>
		/// <param name="Env">Environment to use.</param>
		/// <param name="UBTExecutable">UBT executable to run.</param>
		/// <param name="CommandLine">Command line to pass on to UBT.</param>
		[Obsolete("Deprecated in UE5.1; function signature has changed")]
		public static void RunUBT(CommandEnvironment Env, string UBTExecutable, string CommandLine)
		{
			if (!FileExists(UBTExecutable))
			{
				throw new AutomationException("Unable to find UBT executable: " + UBTExecutable);
			}

			FileReference LogLocation = GetUBTLogLocationFromArgs(Env, CommandLine);
			CommandLine += String.Format(" -log=\"{0}\"", LogLocation);

			IProcessResult Result = Run(UBTExecutable, CommandLine, Options: ERunOptions.AllowSpew | ERunOptions.NoStdOutCapture);
			if (Result.ExitCode != 0)
			{
				throw new AutomationException((ExitCode)Result.ExitCode, "UnrealBuildTool failed. See log for more details. ({0})", CommandUtils.CombinePaths(Env.FinalLogFolder, LogLocation.GetFileName())) { OutputFormat = AutomationExceptionOutputFormat.Minimal };
			}
		}

		/// <summary>
		/// Runs UBT with the specified command line. Automatically creates a logfile. When 
		/// no LogName is specified, the executable name is used as logfile base name.
		/// </summary>
		/// <param name="Env">Environment to use.</param>
		/// <param name="UnrealBuildToolDll">UBT dll to run.</param>
		/// <param name="CommandLine">Command line to pass on to UBT.</param>
		public static void RunUBT(CommandEnvironment Env, FileReference UnrealBuildToolDll, string CommandLine)
		{
			if (!FileReference.Exists(UnrealBuildToolDll))
			{
				throw new AutomationException($"Unable to find UnrealBuildTool.dll: {UnrealBuildToolDll}");
			}

			FileReference LogLocation = GetUBTLogLocationFromArgs(Env, CommandLine);
			CommandLine += String.Format(" -log=\"{0}\"", LogLocation);

			CommandLine = $"\"{UnrealBuildToolDll}\" {CommandLine}";
			IProcessResult Result = Run(Unreal.DotnetPath.FullName, CommandLine, Options: ERunOptions.AllowSpew | ERunOptions.NoStdOutCapture);
			if (Result.ExitCode != 0)
			{
				throw new AutomationException((ExitCode)Result.ExitCode, "UnrealBuildTool failed. See log for more details. ({0})", CommandUtils.CombinePaths(Env.FinalLogFolder, LogLocation.GetFileName())) { OutputFormat = AutomationExceptionOutputFormat.Minimal };
			}
		}

		/// <summary>
		/// Generates a log file name from a UBT command line
		/// </summary>
		/// <param name="Env">Environment to use.</param>
		/// <param name="CommandLine">Command line to pass on to UBT.</param>
		/// <returns></returns>
		/// <exception cref="AutomationException">Exception if a log file cannot be named after many attempts</exception>
		private static FileReference GetUBTLogLocationFromArgs(CommandEnvironment Env, string CommandLine)
		{
			string[] Arguments = SharedUtils.ParseCommandLine(CommandLine);
			string BaseLogName = String.Join("-", Arguments.Where(x => !x.Contains('/') && !x.Contains('\\') && !x.StartsWith("-")));

			// Look for the first -Target= argument
			if (String.IsNullOrWhiteSpace(BaseLogName))
			{
				string TargetArgument = Arguments.FirstOrDefault(x => x.StartsWith("-Target=", StringComparison.OrdinalIgnoreCase));
				if (!String.IsNullOrEmpty(TargetArgument))
				{
					string[] TargetArguments = SharedUtils.ParseCommandLine(TargetArgument["-Target=".Length..].Trim());
					BaseLogName = String.Join("-", TargetArguments.Where(x => !x.Contains('/') && !x.Contains('\\') && !x.StartsWith("-")));
				}
			}

			if (String.IsNullOrWhiteSpace(BaseLogName))
			{
				BaseLogName = "Log";
			}

			string LogName;
			for (int Attempt = 1; ; Attempt++)
			{
				LogName = String.Format("UBA-{0}.txt", (Attempt == 1) ? BaseLogName : String.Format("{0}_{1}", BaseLogName, Attempt));

				FileReference LogLocation = FileReference.Combine(new DirectoryReference(Env.LogFolder), LogName);
				if (!FileReference.Exists(LogLocation))
				{
					return LogLocation;
				}

				if (Attempt >= 100)
				{
					throw new AutomationException("Unable to find name for UBT log file after {0} attempts", Attempt);
				}
			}
		}

		/// <summary>
		/// Builds a UBT Command line.
		/// </summary>
		/// <param name="Project">Unreal project to build (optional)</param>
		/// <param name="Target">Target to build.</param>
		/// <param name="Platform">Platform to build for.</param>
		/// <param name="Config">Configuration to build.</param>
		/// <param name="AdditionalArgs">Additional arguments to pass on to UBT.</param>
		public static string UBTCommandline(FileReference Project, string Target, UnrealTargetPlatform Platform, UnrealTargetConfiguration Config, string AdditionalArgs = "")
		{
			string CmdLine;
			if (Project == null)
			{
				CmdLine = String.Format("{0} {1} {2} {3}", Target, Platform, Config, AdditionalArgs);
			}
			else
			{
				CmdLine = String.Format("{0} {1} {2} -Project={3} {4}", Target, Platform, Config, CommandUtils.MakePathSafeToUseWithCommandLine(Project.FullName), AdditionalArgs);
			}
			return CmdLine;
		}

		/// <summary>
		/// Builds a target using UBT.  Automatically creates a logfile. When 
		/// no LogName is specified, the executable name is used as logfile base name.
		/// </summary>
		/// <param name="Env">BuildEnvironment to use.</param>
		/// <param name="UBTExecutable">UBT executable to run.</param>
		/// <param name="Project">Unreal project to build (optional)</param>
		/// <param name="Target">Target to build.</param>
		/// <param name="Platform">Platform to build for.</param>
		/// <param name="Config">Configuration to build.</param>
		/// <param name="AdditionalArgs">Additional arguments to pass on to UBT.</param>
		[Obsolete("Deprecated in UE5.1; Use the alternate version below with the UnrealBuildTool.dll, not executable")]
		public static void RunUBT(CommandEnvironment Env, string UBTExecutable, FileReference Project, string Target, UnrealTargetPlatform Platform, UnrealTargetConfiguration Config, string AdditionalArgs = "")
		{
			RunUBT(Env, UBTExecutable, UBTCommandline(Project, Target, Platform, Config, AdditionalArgs));
		}

		/// <summary>
		/// Builds a target using UBT.  Automatically creates a logfile. When 
		/// no LogName is specified, the executable name is used as logfile base name.
		/// </summary>
		/// <param name="Env">BuildEnvironment to use.</param>
		/// <param name="UnrealBuildToolDll">UBT dll to run.</param>
		/// <param name="Project">Unreal project to build (optional)</param>
		/// <param name="Target">Target to build.</param>
		/// <param name="Platform">Platform to build for.</param>
		/// <param name="Config">Configuration to build.</param>
		/// <param name="AdditionalArgs">Additional arguments to pass on to UBT.</param>
		public static void RunUBT(CommandEnvironment Env, FileReference UnrealBuildToolDll, FileReference Project, string Target, UnrealTargetPlatform Platform, UnrealTargetConfiguration Config, string AdditionalArgs = "")
		{
			RunUBT(Env, UnrealBuildToolDll, UBTCommandline(Project, Target, Platform, Config, AdditionalArgs));
		}
	}
}