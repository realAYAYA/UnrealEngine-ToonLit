// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DataLayerEditor : ModuleRules
{
	public DataLayerEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"EditorFramework",
				"EditorWidgets",
				"EditorSubsystem",
				"PropertyEditor",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"SceneOutliner",
				"ToolMenus",
				"AssetTools",
				"ContentBrowserData"
			}
		);
	}
}
