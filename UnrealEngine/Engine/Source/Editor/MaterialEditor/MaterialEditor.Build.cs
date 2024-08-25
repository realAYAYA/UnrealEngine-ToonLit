// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MaterialEditor : ModuleRules
{
	public MaterialEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] 
			{
				"AssetTools",
				"Kismet",
				"EditorWidgets",
				"MessageLog",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "AppFramework",
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"InputCore",
				"Engine",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"RenderCore",
				"RHI",
                "MaterialUtilities",
                "PropertyEditor",
				"EditorFramework",
				"UnrealEd",
				"GraphEditor",
                "AdvancedPreviewScene",
                "Projects",
                "AssetRegistry",
				"ToolMenus",
				"MainFrame",
				"Landscape"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"SceneOutliner",
				"ClassViewer",
				"ContentBrowser",
				"WorkspaceMenuStructure"
			}
		);
	}
}
