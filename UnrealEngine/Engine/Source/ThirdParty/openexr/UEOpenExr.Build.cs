// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class UEOpenExr : ModuleRules
{
	public UEOpenExr(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicDependencyModuleNames.Add("Imath");

		bool bDebug = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT);

		string DeploymentDirectory = Path.Combine(ModuleDirectory, "Deploy", "openexr-3.1.5");

		PublicIncludePaths.Add(Path.Combine(DeploymentDirectory, "include"));

		// XXX: OpenEXR includes some of its own headers without the
		// leading "OpenEXR/..."
		PublicIncludePaths.Add(Path.Combine(DeploymentDirectory, "include", "OpenEXR"));

		string LibPostfix = bDebug ? "_d" : "";

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName(),
				Target.WindowsPlatform.GetArchitectureSubpath(),
				"lib");

			PublicAdditionalLibraries.AddRange(
				new string[] {
					Path.Combine(LibDirectory, "Iex-3_1" + LibPostfix + ".lib"),
					Path.Combine(LibDirectory, "IlmThread-3_1" + LibPostfix + ".lib"),
					Path.Combine(LibDirectory, "OpenEXR-3_1" + LibPostfix + ".lib"),
					Path.Combine(LibDirectory, "OpenEXRCore-3_1" + LibPostfix + ".lib"),
					Path.Combine(LibDirectory, "OpenEXRUtil-3_1" + LibPostfix + ".lib")
				}
			);
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"Mac",
				"lib");

			PublicAdditionalLibraries.AddRange(
				new string[] {
					Path.Combine(LibDirectory, "libIex-3_1" + LibPostfix + ".a"),
					Path.Combine(LibDirectory, "libIlmThread-3_1" + LibPostfix + ".a"),
					Path.Combine(LibDirectory, "libOpenEXR-3_1" + LibPostfix + ".a"),
					Path.Combine(LibDirectory, "libOpenEXRCore-3_1" + LibPostfix + ".a"),
					Path.Combine(LibDirectory, "libOpenEXRUtil-3_1" + LibPostfix + ".a")
				}
			);
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"Unix",
				Target.Architecture,
				"lib");

			PublicAdditionalLibraries.AddRange(
				new string[] {
					Path.Combine(LibDirectory, "libIex-3_1" + LibPostfix + ".a"),
					Path.Combine(LibDirectory, "libIlmThread-3_1" + LibPostfix + ".a"),
					Path.Combine(LibDirectory, "libOpenEXR-3_1" + LibPostfix + ".a"),
					Path.Combine(LibDirectory, "libOpenEXRCore-3_1" + LibPostfix + ".a"),
					Path.Combine(LibDirectory, "libOpenEXRUtil-3_1" + LibPostfix + ".a")
				}
			);
		}
	}
}
