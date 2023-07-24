// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CacheTrackRecorder : ModuleRules
{
	public CacheTrackRecorder(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add(ModuleDirectory + "/Public");
		
		PrivateDependencyModuleNames.AddRange(
			new[] {
				"Core",
				"CoreUObject",
				"Engine",
				"LevelEditor",
				"LevelSequence",
				"LevelSequenceEditor",
				"MovieScene",
				"TakesCore",
				"TakeMovieScene",
				"Settings",
				"Slate",
				"SlateCore",
				"UnrealEd",
			}
		);

		PublicDependencyModuleNames.AddRange(
			new[] {
                "TakeTrackRecorders",
                "Sequencer",
            }
        );
    }
}
