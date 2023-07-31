// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class Blosc : ModuleRules
{
	public Blosc(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		bool bDebug = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT);

		string DeploymentDirectory = Path.Combine(ModuleDirectory, "Deploy", "c-blosc-1.21.0");

		PublicIncludePaths.Add(Path.Combine(DeploymentDirectory, "include"));

		string LibPostfix = bDebug ? "_d" : "";

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName(),
				Target.WindowsPlatform.GetArchitectureSubpath(),
				"lib");

			PublicAdditionalLibraries.Add(Path.Combine(LibDirectory, "libblosc" + LibPostfix + ".lib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"Mac",
				"lib");

			PublicAdditionalLibraries.Add(Path.Combine(LibDirectory, "libblosc" + LibPostfix + ".a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"Unix",
				Target.Architecture,
				"lib");

			PublicAdditionalLibraries.Add(Path.Combine(LibDirectory, "libblosc" + LibPostfix + ".a"));
		}
	}
}
