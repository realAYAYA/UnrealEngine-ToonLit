// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Renderer : ModuleRules
{
	public Renderer(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				"../Shaders/Shared"
				}
			);

		PublicDependencyModuleNames.Add("Core");
        PublicDependencyModuleNames.Add("Engine");
        PublicDependencyModuleNames.Add("MaterialShaderQualitySettings");

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
				"RHI"
            }
            );

        PrivateIncludePathModuleNames.AddRange(new string[] { "HeadMountedDisplay" });
        DynamicallyLoadedModuleNames.AddRange(new string[] { "HeadMountedDisplay" });
		PrivateIncludePathModuleNames.AddRange(new string[] { "EyeTracker" });
		DynamicallyLoadedModuleNames.AddRange(new string[] { "EyeTracker" });
	}
}
