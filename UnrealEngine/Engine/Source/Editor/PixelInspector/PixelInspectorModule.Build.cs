// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

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
		var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

		PrivateIncludePaths.AddRange(
			new string[] {
				// required for PostProcessing.
				Path.Combine(EngineDir, "Source/Runtime/Renderer/Private")
			}
		);

		PrivateDependencyModuleNames.AddRange(
             new string[] {
					"Engine",
                    "UnrealEd"
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
                    "UnrealEd",
    				"SlateCore",
    				"Slate",
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
