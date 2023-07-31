// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FbxAutomationTestBuilder : ModuleRules
{
    public FbxAutomationTestBuilder(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicDependencyModuleNames.AddRange
        (
            new string[] {
				"Core",
                "InputCore",
                "CoreUObject",
				"Slate",
                
                "Engine",
                "UnrealEd",
                "PropertyEditor",
				"LevelEditor"
            }
        );
        
        PrivateDependencyModuleNames.AddRange(
             new string[] {
					"Engine",
                    "UnrealEd",
                    "WorkspaceMenuStructure"
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
        }
	}
}
