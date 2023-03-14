// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TakeTrackRecorders : ModuleRules
{
	public TakeTrackRecorders(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "Engine",
                "MovieScene",
                "MovieSceneTracks",
				"TakesCore",
				"TimeManagement",
				"SequenceRecorder", // For access to the individual UProperty type recorders
				"SerializedRecorderInterface",
				"UnrealEd",
            }
        );
	}
}
