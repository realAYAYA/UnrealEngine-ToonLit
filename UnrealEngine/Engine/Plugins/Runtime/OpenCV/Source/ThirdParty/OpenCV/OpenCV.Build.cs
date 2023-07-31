// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;
using UnrealBuildTool;

public class OpenCV : ModuleRules
{
	public OpenCV(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string PlatformDir = Target.Platform.ToString();
		string IncPath = Path.Combine(ModuleDirectory, "include");
        string BinaryPath = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../../Binaries/ThirdParty", PlatformDir));
		
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemIncludePaths.Add(IncPath);

			string LibPath = Path.Combine(ModuleDirectory, "lib", PlatformDir);
			string LibName = "opencv_world455";

			if (Target.Configuration == UnrealTargetConfiguration.Debug &&
				Target.bDebugBuildsActuallyUseDebugCRT)
			{
					LibName += "d";
			}

			PublicAdditionalLibraries.Add(Path.Combine(LibPath, LibName + ".lib"));
			string DLLName = LibName + ".dll";
			PublicDelayLoadDLLs.Add(DLLName);
			RuntimeDependencies.Add(Path.Combine(BinaryPath, DLLName));
			PublicDefinitions.Add("WITH_OPENCV=1");
			PublicDefinitions.Add("OPENCV_PLATFORM_PATH=Binaries/ThirdParty/" + PlatformDir);
			PublicDefinitions.Add("OPENCV_DLL_NAME=" + DLLName);
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicSystemIncludePaths.Add(IncPath);
			
			string LibName = "libopencv_world.so";
			PublicAdditionalLibraries.Add(Path.Combine(BinaryPath, LibName));
			PublicRuntimeLibraryPaths.Add(BinaryPath);
			RuntimeDependencies.Add(Path.Combine(BinaryPath, LibName));
			RuntimeDependencies.Add(Path.Combine(BinaryPath, "libopencv_world.so.405"));
			PublicDefinitions.Add("WITH_OPENCV=1");
		}
		else // unsupported platform
		{
            PublicDefinitions.Add("WITH_OPENCV=0");
		}
	}
}
