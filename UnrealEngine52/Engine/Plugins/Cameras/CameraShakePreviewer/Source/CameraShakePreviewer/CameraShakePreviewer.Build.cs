// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CameraShakePreviewer : ModuleRules
{
	public CameraShakePreviewer(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"AssetRegistry",
				"ContentBrowser",
				"WorkspaceMenuStructure",
				}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ApplicationCore",
				"SlateCore",
				"AppFramework",
				"GameplayCameras",
				"LevelEditor",
				"PropertyEditor",
				"Slate",
				"ToolMenus",
				"EditorFramework",
				"UnrealEd",
			}
		);

		PublicDependencyModuleNames.AddRange(new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
			}
		);
    }
}

