// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CustomizableSequencerTracksEditor : ModuleRules
{
	public CustomizableSequencerTracksEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.AddRange(
			new string[] {
				"CustomizableSequencerTracksEditor/Private",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
				"Core",
				"CoreUObject",
				"CustomizableSequencerTracks",
				"EditorFramework",
				"MovieScene",
				"Sequencer",
				"SlateCore",
				"Slate",
				"UnrealEd",
			}
		);
	}
}
