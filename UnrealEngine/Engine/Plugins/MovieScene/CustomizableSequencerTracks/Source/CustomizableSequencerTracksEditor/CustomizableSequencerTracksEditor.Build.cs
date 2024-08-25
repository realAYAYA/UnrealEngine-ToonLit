// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CustomizableSequencerTracksEditor : ModuleRules
{
	public CustomizableSequencerTracksEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
				"Core",
				"CoreUObject",
				"CustomizableSequencerTracks",
				"EditorFramework",
				"MovieScene",
				"SequencerCore",
				"Sequencer",
				"SlateCore",
				"Slate",
				"UnrealEd",
			}
		);

		ShortName = "CustomSeqTrEd";
	}
}
