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
				"PropertyEditor"
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
				"AssetRegistry",
				"AssetTools",
				"CoreUObject",
				"ContentBrowser",
				"Engine",
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
				"UnrealEd",
				"WorkspaceMenuStructure"
			}
		);
	}
}
