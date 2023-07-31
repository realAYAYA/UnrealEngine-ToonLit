// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;
using EpicGames.Core;

public class PLCrashReporter : ModuleRules
{
	public PLCrashReporter(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string PLGitRepoRoot = "PLCrashReporter";

		string[] PLDefines = new string[] {};

		string PLCrashReporterPath = Path.Combine(Target.UEThirdPartySourceDirectory,"PLCrashReporter");
		string PLSourcePath = Path.Combine(PLCrashReporterPath,PLGitRepoRoot,"Source");
		string LibConfig = "Release";
		string DefaultLibFolder = "lib-Xcode-12.4";
		string PLLibPath = Path.Combine(PLCrashReporterPath, "lib", DefaultLibFolder);

		if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicSystemIncludePaths.Add(PLSourcePath);

			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				LibConfig = "Debug";
			}
			else
			{
				LibConfig = "Release";
			}

			string Lib = Path.Combine(PLLibPath, Target.Platform.ToString(), LibConfig, "libCrashReporter.a");
			PublicAdditionalLibraries.Add(Lib);
			PublicDefinitions.AddRange(PLDefines);	
		}
    }
}
