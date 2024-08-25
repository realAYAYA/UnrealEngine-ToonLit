// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Text;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	/// <summary>
	/// Base class for platform-specific project generators
	/// </summary>
	class WindowsProjectGenerator : PlatformProjectGenerator
	{
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Arguments">Command line arguments passed to the project generator</param>
		/// <param name="Logger">Logger for output</param>
		public WindowsProjectGenerator(CommandLineArguments Arguments, ILogger Logger)
			: base(Arguments, Logger)
		{
		}

		/// <inheritdoc/>
		public override IEnumerable<UnrealTargetPlatform> GetPlatforms()
		{
			yield return UnrealTargetPlatform.Win64;
		}

		/// <inheritdoc/>
		public override string GetVisualStudioPlatformName(VSSettings InVSSettings)
		{
			if (InVSSettings.Platform == UnrealTargetPlatform.Win64)
			{
				if (InVSSettings.Architecture == UnrealArch.Arm64)
				{
					return "arm64";
				}
				else if (InVSSettings.Architecture == UnrealArch.Arm64ec)
				{
					return "arm64ec";
				}
				return "x64";
			}
			return InVSSettings.Platform.ToString();
		}

		/// <inheritdoc/>
		public override string GetVisualStudioUserFileStrings(VisualStudioUserFileSettings VCUserFileSettings, VSSettings InVSSettings, string InConditionString, TargetRules InTargetRules, FileReference TargetRulesPath, FileReference ProjectFilePath, FileReference? NMakeOutputPath, string ProjectName, string? ForeignUProjectPath)
		{
			StringBuilder VCUserFileContent = new StringBuilder();

			string LocalOrRemoteString = InVSSettings.Architecture == null || InVSSettings.Architecture.Value == UnrealArch.Host.Value
				? "Local" : "Remote";

			VCUserFileContent.AppendLine("  <PropertyGroup {0}>", InConditionString);
			if (InTargetRules.Type != TargetType.Game)
			{
				string DebugOptions = "";

				if (ForeignUProjectPath != null)
				{
					DebugOptions += ForeignUProjectPath;
					DebugOptions += " -skipcompile";
				}
				else if (InTargetRules.Type == TargetType.Editor && InTargetRules.ProjectFile != null)
				{
					DebugOptions += ProjectName;
				}

				VCUserFileContent.AppendLine($"    <{LocalOrRemoteString}DebuggerCommandArguments>{DebugOptions}</{LocalOrRemoteString}DebuggerCommandArguments>");
			}

			VCUserFileContent.AppendLine($"    <DebuggerFlavor>Windows{LocalOrRemoteString}Debugger</DebuggerFlavor>");
			VCUserFileContent.AppendLine("  </PropertyGroup>");

			return VCUserFileContent.ToString();
		}

		/// <inheritdoc/>
		public override bool RequiresVSUserFileGeneration()
		{
			return true;
		}

		/// <inheritdoc/>
		public override IList<string> GetSystemIncludePaths(UEBuildTarget InTarget)
		{
			List<string> Result = new List<string>();
			foreach (DirectoryReference Path in InTarget.Rules.WindowsPlatform.Environment!.IncludePaths)
			{
				Result.Add(Path.FullName);
			}

			return Result;
		}
	}
}
