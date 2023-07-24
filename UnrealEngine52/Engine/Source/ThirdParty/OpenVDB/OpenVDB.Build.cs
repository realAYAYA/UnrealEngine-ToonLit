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

		// OpenVDB makes use of both RTTI and C++ exceptions, so clients of
		// this module should likely enable both of these as well.
		bUseRTTI = true;
		bEnableExceptions = true;

		string DeploymentDirectory = Path.Combine(ModuleDirectory, "Deploy", "openvdb-8.1.0");

		PublicSystemIncludePaths.Add(Path.Combine(DeploymentDirectory, "include"));

		PublicDefinitions.Add("OPENVDB_STATICLIB");
		PublicDefinitions.Add("OPENVDB_OPENEXR_STATICLIB");
		PublicDefinitions.Add("NOMINMAX");

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName(),
				Target.Architecture.WindowsName,
				"lib");

			PublicAdditionalLibraries.Add(Path.Combine(LibDirectory, "libopenvdb.lib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"Mac",
				"lib");

			PublicAdditionalLibraries.Add(Path.Combine(LibDirectory, "libopenvdb.a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"Unix",
				Target.Architecture.LinuxName,
				"lib");

			PublicAdditionalLibraries.Add(Path.Combine(LibDirectory, "libopenvdb.a"));
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
