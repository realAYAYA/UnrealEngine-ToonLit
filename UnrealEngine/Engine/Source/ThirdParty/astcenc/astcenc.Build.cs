// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class astcenc : ModuleRules
{
	protected readonly string Version = "4.2.0";
	protected string VersionPath { get => Path.Combine(ModuleDirectory, Version); }
	protected string LibraryPath { get => Path.Combine(VersionPath, "lib"); }

	public astcenc(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicDefinitions.Add("WITH_ASTC_ENCODER=1");
		PublicSystemIncludePaths.Add(Path.Combine(VersionPath, "Source"));
		
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			bool bUseDebugLibs = Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT;
			string ConfigName = bUseDebugLibs ? "Debug" : "Release";

			PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Win64", ConfigName, "astcenc-sse4.1-static.lib")); 
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Mac", "Release", "libastcenc-static.a"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Linux", "Release", "libastcenc-sse4.1-static.a"));
		}
	}
}

