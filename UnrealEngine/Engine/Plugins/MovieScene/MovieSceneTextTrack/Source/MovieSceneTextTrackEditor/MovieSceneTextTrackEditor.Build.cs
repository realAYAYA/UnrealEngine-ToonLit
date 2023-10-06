// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MovieSceneTextTrackEditor : ModuleRules
{
	public MovieSceneTextTrackEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"MovieScene",
				"MovieSceneTextTrack",
				"MovieSceneTools",
				"MovieSceneTracks",
				"Sequencer",
				"SequencerCore",
				"Slate",
				"SlateCore",
				"UnrealEd",
			}
		);
	}
}
