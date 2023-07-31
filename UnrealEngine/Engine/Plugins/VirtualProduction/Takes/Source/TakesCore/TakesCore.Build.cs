// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TakesCore : ModuleRules
{
	public TakesCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"EditorFramework",
				"UnrealEd",
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
				"LevelSequence",
				"SlateCore",
                "MovieScene",
                "MovieSceneTracks",
                "MovieSceneTools",
				"LevelSequence",
				"Engine",
                "SerializedRecorderInterface",

            }
        );
	}
}
