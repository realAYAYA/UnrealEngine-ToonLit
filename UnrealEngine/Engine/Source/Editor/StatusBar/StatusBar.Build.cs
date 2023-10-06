// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StatusBar : ModuleRules
{
	public StatusBar(ReadOnlyTargetRules Target)
		 : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"EditorFramework",
				"EditorStyle",
				"EditorSubsystem",
				"Engine",
				"InputCore",
				"Slate",
				"SlateCore",
				"InputCore",
				"EditorFramework",
				"SourceControlWindows",
				"UnsavedAssetsTracker",
				"ToolMenus",
				"ToolWidgets",
				"UnrealEd",
				"AssetTools",
				"SourceControl"
			});

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"MainFrame",
				"OutputLog",
				"ContentBrowser", 
			});

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"MainFrame",
				"OutputLog",
				"ContentBrowser",
			});
	}
}
