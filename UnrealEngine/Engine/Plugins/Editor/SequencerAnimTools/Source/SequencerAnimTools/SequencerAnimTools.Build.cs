// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SequencerAnimTools: ModuleRules
{
	public SequencerAnimTools (ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "Slate",
                "SlateCore",
			}
		);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"InputCore",
				"UnrealEd",
				"LevelEditor",
				
				"EditorFramework",
                "EditorInteractiveToolsFramework",
                "InteractiveToolsFramework",

                "MovieScene",
                "MovieSceneTracks",
                "MovieSceneTools",
                "Sequencer",
				"LevelSequence",
				"LevelSequenceEditor",

				"ControlRig",
				"ControlRigEditor"
			}
        );
	}
}
