// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WorldBrowser : ModuleRules
{
    public WorldBrowser(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "AssetRegistry",
				"AssetTools",
                "MeshUtilities",
                "MeshMergeUtilities",
            }
        );
     
        PrivateDependencyModuleNames.AddRange(
            new string[] {
				"ApplicationCore",
                "AppFramework",
                "Core", 
                "CoreUObject",
                "RenderCore",
                "InputCore",
                "Engine",
				"Landscape",
                "Slate",
				"SlateCore",
                "EditorWidgets",
                "ToolWidgets",
				"EditorFramework",
				"UnrealEd",
                "GraphEditor",
                "LevelEditor",
                "PropertyEditor",
                "DesktopPlatform",
                "MainFrame",
                "SourceControl",
				"SourceControlWindows",
                "MeshDescription",
				"StaticMeshDescription",
				"NewLevelDialog",
				"LandscapeEditor",
                "FoliageEdit",
                "ImageWrapper",
                "Foliage",
                "MaterialUtilities",
                "RHI",
                "Json",
				"ToolMenus",
				"TypedElementRuntime",
				"TypedElementFramework",
			}
		);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "AssetRegistry",
				"AssetTools",
				"SceneOutliner",
                "MeshUtilities",
                "ContentBrowser",
                "MeshMergeUtilities",
            }
		);
    }
}
