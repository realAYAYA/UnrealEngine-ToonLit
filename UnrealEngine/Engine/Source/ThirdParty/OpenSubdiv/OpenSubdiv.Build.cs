// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class OpenSubdiv : ModuleRules
{
	public OpenSubdiv(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		bool bDebug = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT);

		string DeploymentDirectory = Path.Combine(ModuleDirectory, "Deploy", "OpenSubdiv-3.6.0");

		PublicSystemIncludePaths.Add(Path.Combine(DeploymentDirectory, "include"));

		string LibPostfix = bDebug ? "_d" : "";

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName(),
				Target.Architecture.WindowsLibDir,
				"lib");

			PublicAdditionalLibraries.Add(
				Path.Combine(LibDirectory, "osdCPU" + LibPostfix + ".lib"));
			PublicAdditionalLibraries.Add(
				Path.Combine(LibDirectory, "osdGPU" + LibPostfix + ".lib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"Mac",
				"lib");

			PublicAdditionalLibraries.Add(
				Path.Combine(LibDirectory, "libosdCPU" + LibPostfix + ".a"));
			PublicAdditionalLibraries.Add(
				Path.Combine(LibDirectory, "libosdGPU" + LibPostfix + ".a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"Unix",
				Target.Architecture.LinuxName,
				"lib");

			// Note that since we no longer support OpenGL on Linux,
			// OpenSubdiv's library for GPU support is not available.
			PublicAdditionalLibraries.Add(
				Path.Combine(LibDirectory, "libosdCPU" + LibPostfix + ".a"));
		}
	}
}
