// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Diagnostics;
using System.Collections.Generic;
using System.IO;

public class OpenVDB : ModuleRules
{
	public OpenVDB(ReadOnlyTargetRules Target) : base(Target)
	{
		// We are just setting up paths for pre-compiled binaries.
		Type = ModuleType.External;

		bool bDebug = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT);

		// The OpenVDB library itself makes use of both RTTI and C++ exceptions, but we
		// don't compile with either of those on Linux, so our wrapper needs to disable them
		bUseRTTI = false;
		bEnableExceptions = false;

		string DeploymentDirectory = Path.Combine(ModuleDirectory, "Deploy", "openvdb-8.1.0");

		PublicSystemIncludePaths.Add(Path.Combine(DeploymentDirectory, "include"));

		PublicDefinitions.Add("OPENVDB_STATICLIB");
		PublicDefinitions.Add("OPENVDB_OPENEXR_STATICLIB");
		PublicDefinitions.Add("NOMINMAX");
		PublicDefinitions.Add("VDB_WITH_EPIC_EXTENSIONS=1");

		string LibPostfix = bDebug ? "_d" : "";

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName(),
				Target.Architecture.WindowsLibDir,
				"lib");

			string StaticLibName = "libopenvdb" + LibPostfix + ".lib";

			PublicAdditionalLibraries.Add(
				Path.Combine(LibDirectory, StaticLibName));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"Mac",
				"lib");

			string StaticLibName = "libopenvdb" + LibPostfix + ".a";

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

			string StaticLibName = "libopenvdb" + LibPostfix + ".a";

			PublicAdditionalLibraries.Add(
				Path.Combine(LibDirectory, StaticLibName));
		}

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Boost"
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"IntelTBB"
			}
		);

		AddEngineThirdPartyPrivateStaticDependencies(Target, "Blosc");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");
	}
}
