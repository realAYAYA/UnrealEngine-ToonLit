// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LevelInstanceEditor : ModuleRules
{
    public LevelInstanceEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateIncludePathModuleNames.AddRange(
            new string[] {
            }
        );
     
        PrivateDependencyModuleNames.AddRange(
            new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Slate",
				"SlateCore",
				"EditorFramework",
				"UnrealEd",
				"LevelEditor",
				"ToolMenus",
				"PropertyEditor",
				"NewLevelDialog",
				"MainFrame",
				"ContentBrowser",
				"AssetTools",
				"ClassViewer",
				"MessageLog",
				"EditorWidgets",
				"DeveloperSettings",
				"SceneOutliner",
				"WorldPartitionEditor",
				"Kismet"
			}
		);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
		    }
		);
    }
}
