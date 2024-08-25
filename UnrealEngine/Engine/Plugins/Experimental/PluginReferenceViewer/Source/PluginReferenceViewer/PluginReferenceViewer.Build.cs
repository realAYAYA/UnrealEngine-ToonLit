// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PluginReferenceViewer : ModuleRules
{
	public PluginReferenceViewer(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"InputCore",
				"Engine",
				"Slate",
				"SlateCore",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AssetManagerEditor",
				"AssetRegistry",
				"EditorFramework",
				"PluginUtils",
				"Projects",
				"ApplicationCore",
				"UnrealEd",
				"GraphEditor",
				"EditorWidgets",
				"ToolMenus",
				"ToolWidgets",
				"PluginBrowser",
			}
		);
	}
}
