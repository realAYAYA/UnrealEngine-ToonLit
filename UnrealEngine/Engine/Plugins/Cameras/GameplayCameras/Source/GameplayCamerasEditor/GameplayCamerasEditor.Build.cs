// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameplayCamerasEditor : ModuleRules
{
	public GameplayCamerasEditor(ReadOnlyTargetRules Target) : base(Target)
	{
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
				"AssetDefinition",
				"AssetRegistry",
				"GameplayCameras",
				"BlueprintGraph",
				"Core",
				"CoreUObject",
				"EditorFramework",
				"Engine",
				"InputCore",
				"Kismet",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"UnrealEd",
				"TimeManagement"
			}
		);

		var DynamicModuleNames = new string[] {
			"LevelEditor",
			"PropertyEditor",
		};

		foreach (var Name in DynamicModuleNames)
		{
			PrivateIncludePathModuleNames.Add(Name);
			DynamicallyLoadedModuleNames.Add(Name);
		}
	}
}

