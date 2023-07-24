// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class LevelSnapshotsEditor : ModuleRules
{
	public LevelSnapshotsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
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
				"EditorFramework",
				"EditorStyle",
				"EditorWidgets",
				"GameProjectGeneration",
				"InputCore",
				"Kismet",
				"LevelSnapshots",
				"LevelSnapshotFilters",
				"Projects",
				"PropertyEditor",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"ToolWidgets",
				"UnrealEd",
				"WorkspaceMenuStructure"
			}
			);

		// TODO: Should not be including private headers from a different module
		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(GetModuleDirectory("GameplayMediaEncoder"), "Private"), // For GameplayMediaEncoderCommon.h
			});
	}
}
