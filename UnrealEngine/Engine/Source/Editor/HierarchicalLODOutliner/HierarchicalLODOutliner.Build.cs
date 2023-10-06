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

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				}
		);



		if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[] {
					"EditorFramework",
    				"SlateCore",
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
