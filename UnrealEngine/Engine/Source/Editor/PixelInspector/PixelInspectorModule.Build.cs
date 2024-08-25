// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PixelInspectorModule : ModuleRules
{
    public PixelInspectorModule(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicDependencyModuleNames.AddRange
        (
            new string[] {
				"Core",
                "InputCore",
                "CoreUObject",
				"RHI",
				"RenderCore",
				"Slate",
                "Engine",
                "UnrealEd",
                "PropertyEditor",
			}
        );

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ColorManagement",
				"Renderer",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"LevelEditor"
			}
		);

		if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[] {
					"EditorFramework",
    				"SlateCore",
                }
            );

            CircularlyReferencedDependentModules.AddRange(
                new string[] {
                    "UnrealEd"
                }
            );
        }
	}
}
