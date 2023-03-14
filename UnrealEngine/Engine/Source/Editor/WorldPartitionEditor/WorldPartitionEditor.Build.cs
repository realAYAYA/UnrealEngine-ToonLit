// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WorldPartitionEditor : ModuleRules
{
    public WorldPartitionEditor(ReadOnlyTargetRules Target) : base(Target)
    {     
        PrivateDependencyModuleNames.AddRange(
            new string[] {
				"Core",
				"CoreUObject",
				"EditorFramework",
				"EditorSubsystem",
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
				"SceneOutliner"
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
				"SceneOutliner",
				"WorkspaceMenuStructure",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(GetModuleDirectory("Renderer"), "Private"),
			}
		);
	}
}
