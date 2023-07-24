// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ObjectMixerEditor : ModuleRules
{
	public ObjectMixerEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"OutputLog",
				"PropertyEditor", 
				"SceneOutliner"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"PlacementMode",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore",
				"AssetRegistry",
				"AssetTools",
				"CoreUObject",
				"ContentBrowser",
				"Engine",
				"EditorConfig",
				"EditorStyle",
				"EditorWidgets",
				"InputCore",
				"Kismet",
				"LevelEditor",
				"Projects",
				"SceneOutliner",
				"Slate",
				"SlateCore",
				"ToolMenus", 
				"ToolWidgets",
				"TypedElementFramework",
				"TypedElementRuntime",
				"UMG",
				"UnrealEd",
				"WorkspaceMenuStructure"
			}
		);
	}
}
