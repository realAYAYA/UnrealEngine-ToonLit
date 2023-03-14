// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	/// <summary>
	/// Queries information about the targets supported by a project
	/// </summary>
	[ToolMode("QueryTargets", ToolModeOptions.XmlConfig | ToolModeOptions.BuildPlatformsHostOnly | ToolModeOptions.SingleInstance)]
	class QueryTargetsMode : ToolMode
	{
		/// <summary>
		/// Path to the project file to query
		/// </summary>
		[CommandLine("-Project=")]
		FileReference? ProjectFile = null;

		/// <summary>
		/// Path to the output file to receive information about the targets
		/// </summary>
		[CommandLine("-Output=")]
		FileReference? OutputFile = null;

		/// <summary>
		/// Execute the mode
		/// </summary>
		/// <param name="Arguments">Command line arguments</param>
		/// <returns></returns>
		/// <param name="Logger"></param>
		public override int Execute(CommandLineArguments Arguments, ILogger Logger)
		{
			Arguments.ApplyTo(this);

			// Create the build configuration object, and read the settings
			BuildConfiguration BuildConfiguration = new BuildConfiguration();
			XmlConfig.ApplyTo(BuildConfiguration);
			Arguments.ApplyTo(BuildConfiguration);

			// Ensure the path to the output file is valid
			if(OutputFile == null)
			{
				OutputFile = GetDefaultOutputFile(ProjectFile);
			}

			// Create the rules assembly
			RulesAssembly Assembly;
			if (ProjectFile != null)
			{
				Assembly = RulesCompiler.CreateProjectRulesAssembly(ProjectFile, BuildConfiguration.bUsePrecompiled, BuildConfiguration.bSkipRulesCompile, BuildConfiguration.bForceRulesCompile, Logger);
			}
			else
			{
				Assembly = RulesCompiler.CreateEngineRulesAssembly(BuildConfiguration.bUsePrecompiled, BuildConfiguration.bSkipRulesCompile, BuildConfiguration.bForceRulesCompile, Logger);
			}

			// Write information about these targets
			WriteTargetInfo(ProjectFile, Assembly, OutputFile, Arguments, Logger);
			Logger.LogInformation("Written {OutputFile}", OutputFile);
			return 0;
		}

		/// <summary>
		/// Gets the path to the target info output file
		/// </summary>
		/// <param name="ProjectFile">Project file being queried</param>
		/// <returns>Path to the output file</returns>
		public static FileReference GetDefaultOutputFile(FileReference? ProjectFile)
		{
			if (ProjectFile == null)
			{
				return FileReference.Combine(Unreal.EngineDirectory, "Intermediate", "TargetInfo.json");
			}
			else
			{
				return FileReference.Combine(ProjectFile.Directory, "Intermediate", "TargetInfo.json");
			}
		}

		/// <summary>
		/// Writes information about the targets in an assembly to a file
		/// </summary>
		/// <param name="ProjectFile">The project file for the targets being built</param>
		/// <param name="Assembly">The rules assembly for this target</param>
		/// <param name="OutputFile">Output file to write to</param>
		/// <param name="Arguments"></param>
		/// <param name="Logger">Logger for output</param>
		public static void WriteTargetInfo(FileReference? ProjectFile, RulesAssembly Assembly, FileReference OutputFile, CommandLineArguments Arguments, ILogger Logger)
		{
			// Construct all the targets in this assembly
			List<string> TargetNames = new List<string>();
			Assembly.GetAllTargetNames(TargetNames, false);

			// Write the output file
			DirectoryReference.CreateDirectory(OutputFile.Directory);
			using (JsonWriter Writer = new JsonWriter(OutputFile))
			{
				Writer.WriteObjectStart();
				Writer.WriteArrayStart("Targets");
				foreach (string TargetName in TargetNames)
				{
					// skip target rules that are platform extension or platform group specializations
					string[] TargetPathSplit = TargetName.Split(new char[]{'_'}, StringSplitOptions.RemoveEmptyEntries );
					if (TargetPathSplit.Length > 1 && (UnrealTargetPlatform.IsValidName(TargetPathSplit.Last()) || UnrealPlatformGroup.IsValidName(TargetPathSplit.Last()) ) )
					{
						continue;
					}

					// Construct the rules object
					TargetRules TargetRules;
					try
					{
						string Architecture = UEBuildPlatform.GetBuildPlatform(BuildHostPlatform.Current.Platform).GetDefaultArchitecture(ProjectFile);
						TargetRules = Assembly.CreateTargetRules(TargetName, BuildHostPlatform.Current.Platform, UnrealTargetConfiguration.Development, Architecture, ProjectFile, Arguments, Logger);
					}
					catch (Exception Ex)
					{
						Logger.LogWarning("Unable to construct target rules for {TargetName}", TargetName);
						Logger.LogDebug("{Ex}", ExceptionUtils.FormatException(Ex));
						continue;
					}

					// Write the target info
					Writer.WriteObjectStart();
					Writer.WriteValue("Name", TargetName);
					Writer.WriteValue("Path", Assembly.GetTargetFileName(TargetName)?.ToString());
					Writer.WriteValue("Type", TargetRules.Type.ToString());
					Writer.WriteObjectEnd();
				}
				Writer.WriteArrayEnd();
				Writer.WriteObjectEnd();
			}
		}
	}
}
