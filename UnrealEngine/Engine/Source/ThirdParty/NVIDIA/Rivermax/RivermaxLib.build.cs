// Copyright Epic Games, Inc. All Rights Reserved.
using System.IO;
using UnrealBuildTool;
using System;

public class RivermaxLib : ModuleRules
{
	public RivermaxLib(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "include"));

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			string SDKPath = Path.Combine(Target.UEThirdPartySourceDirectory, "NVIDIA/Rivermax");
			string LibraryPath = Path.Combine(SDKPath, "lib/Win64");
			string LibraryName = "rivermax.dll";
			PublicIncludePaths.Add(Path.Combine(SDKPath,"include"));
			PublicRuntimeLibraryPaths.Add(LibraryPath);
			PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "rivermax.lib"));

			//This is required because dll is not staged otherwise and won't be part of the package
			RuntimeDependencies.Add(Path.Combine(LibraryPath, LibraryName));

			//This is required because Rivermax depends on other drivers / dll to be installed for mellanox. We will manually load the dll and gracefully fail instead of 
			//failing to load the module entirely.
			PublicDelayLoadDLLs.Add(LibraryName);

			//Used during manual loading of the library
			PublicDefinitions.Add("RIVERMAX_LIBRARY_PLATFORM_PATH=" + "Win64");
			PublicDefinitions.Add("RIVERMAX_LIBRARY_NAME=" + LibraryName);
		}
	}
}

