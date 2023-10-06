// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FunctionalTestingEditor : ModuleRules
{
    public FunctionalTestingEditor(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicDependencyModuleNames.AddRange
        (
            new string[] {
				"Core",
                "InputCore",
                "CoreUObject",
                "SlateCore",
                "Slate",
                
                "Engine",
                "AssetRegistry"
			}
        );
        
        PrivateDependencyModuleNames.AddRange(
             new string[] {
				"EditorFramework",
                "UnrealEd",
                "LevelEditor",
                "SessionFrontend",
                "FunctionalTesting",
                "PlacementMode",
                "WorkspaceMenuStructure",
                "ScreenShotComparisonTools",
                "ToolMenus",
				"AssetTools"
            }
         );
	}
}
