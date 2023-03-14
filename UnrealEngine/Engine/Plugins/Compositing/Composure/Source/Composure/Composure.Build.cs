// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Composure : ModuleRules
	{
		public Composure(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Engine",
				}
				);
            
            PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"CameraCalibrationCore",
					"CinematicCamera",
					"Core",
                    "CoreUObject",
                    "Engine",
					"MediaIOCore",
					"MovieScene",
					"MovieSceneTracks",
					"OpenColorIO",
					"TimeManagement",
                }
				);

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
					// Removed dependency, until the MediaFrameworkUtilities plugin is available for all platforms
                    //"MediaFrameworkUtilities",

					"ImageWriteQueue",
					"MediaAssets",
					"MovieSceneCapture",
					"RHI",
				}
				);

            if (Target.bBuildEditor == true)
            {
                PrivateDependencyModuleNames.AddRange(
					new string[]
                    {
						"ActorLayerUtilities",
						"EditorFramework",
						"Slate",
						"SlateCore",
						"UnrealEd",
					}
					);
            }
        }
    }
}
