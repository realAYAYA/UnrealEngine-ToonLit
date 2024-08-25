// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SceneOutliner : ModuleRules
{
	public SceneOutliner(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"ApplicationCore",
				"InputCore",
				"Slate",
				"SlateCore",
				"EditorFramework",
				"UnrealEd",
				"ToolMenus",
				"SourceControl",
				"EditorConfig",
				"SourceControlWindows",
				"UncontrolledChangelists",
				"EditorWidgets",
				"ToolWidgets",
				"UnsavedAssetsTracker",
				"AssetDefinition",
				"TypedElementFramework",
				"TypedElementRuntime",
			}
		);
		
	}
}
