// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OpenXRAR : ModuleRules
{
    public OpenXRAR(ReadOnlyTargetRules Target) : base(Target)
    {
		var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
		PrivateIncludePaths.AddRange(
			new string[] {
					"OpenXRHMD/Private",
					EngineDir + "/Source/Runtime/Renderer/Private",
					EngineDir + "/Source/ThirdParty/OpenXR/include",
				// ... add other private include paths required here ...
			}
			);

		PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "Engine",
				"MRMesh"
				// ... add other public dependencies that you statically link with here ...
			}
			);

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
				"ApplicationCore",
                "HeadMountedDisplay",
                "AugmentedReality",
                "RenderCore",
                "RHI",
				"Projects",
				"OpenXRHMD",
				// ... add private dependencies that you statically link with here ...
			}
            );
			
		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
		
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
		}

		PublicIncludePathModuleNames.Add("OpenXR");

	}
}