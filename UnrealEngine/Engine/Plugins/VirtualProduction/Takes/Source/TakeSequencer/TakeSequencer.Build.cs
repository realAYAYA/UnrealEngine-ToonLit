// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TakeSequencer : ModuleRules
{
	public TakeSequencer(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				
				"Slate",
				"SlateCore",
				"UnrealEd"
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
                "MovieScene",
                "MovieSceneTracks",
                "MovieSceneTools",
				"Sequencer",
				"TakeMovieScene",
				"Engine",
            }
        );
	}
}
