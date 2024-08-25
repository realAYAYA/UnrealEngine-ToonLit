// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using UnrealBuildTool;

public class AlembicLib : ModuleRules
{
	public AlembicLib(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicDependencyModuleNames.Add("Imath");

		bool bDebug = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT);

		string DeploymentDirectory = Path.Combine(ModuleDirectory, "Deploy", "alembic-1.8.6");

		PublicSystemIncludePaths.Add(Path.Combine(DeploymentDirectory, "include"));

		string LibPostfix = bDebug ? "_d" : "";

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName(),
				Target.Architecture.WindowsLibDir,
				"lib");

			string StaticLibName = "Alembic" + LibPostfix + ".lib";

			PublicAdditionalLibraries.Add(
				Path.Combine(LibDirectory, StaticLibName));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"Mac",
				"lib");

			string StaticLibName = "libAlembic" + LibPostfix + ".a";

			PublicAdditionalLibraries.Add(
				Path.Combine(LibDirectory, StaticLibName));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"Unix",
				Target.Architecture.LinuxName,
				"lib");

			string StaticLibName = "libAlembic" + LibPostfix + ".a";

			PublicAdditionalLibraries.Add(
				Path.Combine(LibDirectory, StaticLibName));
		}
	}
}
