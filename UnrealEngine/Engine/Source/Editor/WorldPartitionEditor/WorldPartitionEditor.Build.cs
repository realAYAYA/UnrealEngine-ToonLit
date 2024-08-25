// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WorldPartitionEditor : ModuleRules
{
    public WorldPartitionEditor(ReadOnlyTargetRules Target) : base(Target)
    {     
        PrivateDependencyModuleNames.AddRange(
            new string[] {
				"ApplicationCore",
				"Core",
				"CoreUObject",
				"EditorFramework",
				"EditorSubsystem",
				"EditorWidgets",
				"Engine",
				"InputCore",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"WorldBrowser",
				"MainFrame",
				"PropertyEditor",
				"DeveloperSettings",
				"ToolMenus",
				"RenderCore",
				"Renderer",
				"RHI",
				"SceneOutliner",
				"DataLayerEditor"
			}
		);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
				"AssetTools",
            }
		);

		PrivateIncludePathModuleNames.AddRange
		(
			new string[]
			{
				"WorkspaceMenuStructure",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
			}
		);
	}
}
