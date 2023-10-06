// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TakeRecorderSources : ModuleRules
{
	public TakeRecorderSources(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "CinematicCamera",
                "Core",
				"CoreUObject",
				"EditorFramework",
				"Engine",

				"AudioCaptureEditor",
				"InputCore",
				"LevelEditor",
				"LevelSequence",
                "LevelSequenceEditor",
                "MovieScene",
				"MovieSceneTracks",
                "SceneOutliner",
				"SequenceRecorder", // For FTimecodeBoneMethod
				"SerializedRecorderInterface",
                "Slate",
				"SlateCore",
				"TakesCore",
				"TakeRecorder",
				"TakeMovieScene",
				"UnrealEd",
			}
		);

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "TakeTrackRecorders",
            }
        );
	}
}
