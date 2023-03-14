// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class HoloLensTargetPlatform : ModuleRules
{
	public HoloLensTargetPlatform(ReadOnlyTargetRules Target) : base(Target)
    {
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
                "CoreUObject",
				"Settings",
                "EngineSettings",
                "TargetPlatform",
				"DesktopPlatform",
				"HTTP",
                "Json"
            }
        );

		// directly include the location of the HoloLensPlatformProperties.h files so that we don't instantiate the Core_HoloLens module for host platform
		PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Platforms/HoloLens/Source/Runtime/Core/Public"));

		PrivateIncludePathModuleNames.Add("Settings");

        int Win10Build = 0;
        Version ver = null;
        if (Version.TryParse(Target.WindowsPlatform.WindowsSdkVersion, out ver))
        {
            Win10Build = ver.Build;
        }

        //there is a WinSDK bug that prevented to include the file into this version
		if(Win10Build != 0 && Win10Build != 16299)
        {
            PrivateDefinitions.Add("APPXPACKAGING_ENABLE=1");
        }
		else
        {
            PrivateDefinitions.Add("APPXPACKAGING_ENABLE=0");
        }

        // compile withEngine
        if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
			PrivateIncludePathModuleNames.Add("TextureCompressor");
		}

		PublicSystemLibraries.Add("shlwapi.lib");
        PublicSystemLibraries.Add("runtimeobject.lib");
	}
}
