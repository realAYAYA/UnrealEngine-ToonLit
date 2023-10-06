// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Renderer : ModuleRules
{
	public Renderer(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				EngineDirectory + "/Shaders/Shared",
				EngineDirectory + "/Shaders/Private", // For HaltonUtilities.ush
				}
			);

		PublicDependencyModuleNames.Add("Core");
        PublicDependencyModuleNames.Add("Engine");

        if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.Add("TargetPlatform");
        }

        // Renderer module builds faster without unity
        // Non-unity also provides faster iteration
		// Not enabled by default as it might harm full rebuild times without XGE
        //bFasterWithoutUnity = true;

        MinFilesUsingPrecompiledHeaderOverride = 4;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CoreUObject", 
				"ApplicationCore",
				"RenderCore", 
				"ImageWriteQueue",
				"RHI",
				"MaterialShaderQualitySettings"
			}
            );

        PrivateIncludePathModuleNames.AddRange(new string[] { "HeadMountedDisplay" });
        DynamicallyLoadedModuleNames.AddRange(new string[] { "HeadMountedDisplay" });
		PrivateIncludePathModuleNames.AddRange(new string[] { "EyeTracker" });
		DynamicallyLoadedModuleNames.AddRange(new string[] { "EyeTracker" });
	}
}
