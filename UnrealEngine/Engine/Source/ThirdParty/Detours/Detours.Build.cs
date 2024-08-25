// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

[SupportedPlatformGroups("Microsoft")]
public class Detours : ModuleRules
{
	public Detours(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string VcRootDir = $"{Target.Architecture}-windows-static{(Target.bUseStaticCRT ? string.Empty : "-md")}";
		string PkgDir = $"detours_{Target.Architecture}-windows-static{(Target.bUseStaticCRT ? string.Empty : "-md")}";

		PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "Windows", VcRootDir, PkgDir, "include"));

		PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "Windows", VcRootDir, PkgDir, "lib", "detours.lib"));
	}
}
