// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ReplayTracksEditor : ModuleRules
{
	public ReplayTracksEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ReplayTracks",
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"MovieScene",
				"MovieSceneTools",
				"MovieSceneTracks",
				"Sequencer",				
				"Slate",
				"SlateCore",
				"UnrealEd",
				"TimeManagement",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"ReplayTracksEditor/Private",
			}
		);
	}
}
