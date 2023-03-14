// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class OpenSubdiv : ModuleRules
{
	public OpenSubdiv(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		bool bDebug = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT);

		string DeploymentDirectory = Path.Combine(ModuleDirectory, "Deploy", "OpenSubdiv-3.4.4");

		PublicIncludePaths.Add(Path.Combine(DeploymentDirectory, "include"));

		string LibPostfix = bDebug ? "_d" : "";

		// @todo mesheditor subdiv: Support other platforms, and older/newer compiler toolchains
		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName(),
				Target.WindowsPlatform.GetArchitectureSubpath(),
				"lib");

			PublicAdditionalLibraries.Add(
				Path.Combine(LibDirectory, "osdCPU" + LibPostfix + ".lib"));
			PublicAdditionalLibraries.Add(
				Path.Combine(LibDirectory, "osdGPU" + LibPostfix + ".lib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			// @todo: build Mac libraries
			// string LibDirectory = Path.Combine(
			// 	DeploymentDirectory,
			// 	"Mac",
			// 	"lib");

			// PublicAdditionalLibraries.Add(
			// 	Path.Combine(LibDirectory, "libosdCPU" + LibPostfix + ".a"));
			// PublicAdditionalLibraries.Add(
			// 	Path.Combine(LibDirectory, "libosdGPU" + LibPostfix + ".a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			// @todo: build Linux libraries
			// string LibDirectory = Path.Combine(
			// 	DeploymentDirectory,
			// 	"Unix",
			// 	Target.Architecture,
			// 	"lib");

			// PublicAdditionalLibraries.Add(
			// 	Path.Combine(LibDirectory, "libosdCPU" + LibPostfix + ".a"));
			// PublicAdditionalLibraries.Add(
			// 	Path.Combine(LibDirectory, "libosdGPU" + LibPostfix + ".a"));
		}
	}
}
