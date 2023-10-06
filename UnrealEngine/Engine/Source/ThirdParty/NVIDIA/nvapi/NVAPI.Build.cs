// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class NVAPI : ModuleRules
{
    public NVAPI(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		/*string nvApiNoRedistLibrary = System.IO.Path.Combine(EngineDirectory,
			"Restricted/NoRedist/Source/ThirdParty/NVIDIA/nvapi/amd64/nvapi64.lib");

		// Check if we should redirect to a beta version of nvapi
		bool bHaveNoRedistnvApi = System.IO.File.Exists(nvApiNoRedistLibrary);
		bool bCompilingForProject = Target.ProjectFile != null;*/
		bool bUseNoRedistnvApi = false;//bHaveNoRedistnvApi && bCompilingForProject;

		string nvApiPath = Path.Combine(Target.UEThirdPartySourceDirectory, "NVIDIA", "nvapi");

		if (bUseNoRedistnvApi)
		{
			nvApiPath = Path.Combine(EngineDirectory, "Restricted", "NoRedist", "Source", "ThirdParty", "NVIDIA", "nvapi");
		}

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) && Target.Architecture.bIsX64)
		{
			PublicSystemIncludePaths.Add(nvApiPath);

			PublicAdditionalLibraries.Add(Path.Combine(nvApiPath, "amd64", "nvapi64.lib"));
			PublicDefinitions.Add("WITH_NVAPI=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_NVAPI=0");
		}
	}
}

