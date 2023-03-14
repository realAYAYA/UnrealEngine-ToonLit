// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HierarchicalLODOutliner : ModuleRules
{
    public HierarchicalLODOutliner(ReadOnlyTargetRules Target) : base(Target)
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
                "HierarchicalLODUtilities",
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
					"ToolMenus",
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
