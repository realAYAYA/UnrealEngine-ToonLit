// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PlacementMode : ModuleRules
{
    public PlacementMode(ReadOnlyTargetRules Target) : base(Target)
    {
		PrivateIncludePathModuleNames.Add("AssetTools");

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
                "ContentBrowser",
				"ContentBrowserData",
                "CollectionManager",
                "LevelEditor",
                "AssetTools",
                "ToolMenus",
            } 
        );
    }
}
