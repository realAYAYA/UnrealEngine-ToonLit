// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TimecodeSynchronizerEditor : ModuleRules
{
	public TimecodeSynchronizerEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"Core",
				"CoreUObject",
				"EditorFramework",
				"Engine",		
				"InputCore",
                "MediaAssets",
                "MediaPlayerEditor",
                "PropertyEditor",
                "SlateCore",
				"Slate",
                "TimecodeSynchronizer",
                "UnrealEd",
			});
	}
}
