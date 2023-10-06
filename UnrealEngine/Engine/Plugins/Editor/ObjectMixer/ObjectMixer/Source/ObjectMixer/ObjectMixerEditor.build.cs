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
				"AdvancedWidgets",
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
				"LevelSequence",
				"Projects",
				"Sequencer",
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
