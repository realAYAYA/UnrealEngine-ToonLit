// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LiveLinkCameraRecording : ModuleRules
{
	public LiveLinkCameraRecording(ReadOnlyTargetRules Target) : base(Target)
	{		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CameraCalibrationCore",
				"CinematicCamera",
				"Core",
				"CoreUObject",
				"Engine",
				"LevelSequence",
				"LiveLinkCamera",
				"LiveLinkComponents",
				"LiveLinkInterface",
				"LiveLinkMovieScene",
				"LiveLinkSequencer",
				"MovieScene",
				"MovieSceneTools",
				"MovieSceneTracks",
				"Sequencer",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"TakeTrackRecorders",
			}
		);
	}
}
