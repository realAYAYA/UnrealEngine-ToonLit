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
				"Slate",
				"SlateCore",
				"InputCore",
				"UnrealEd",
				"LevelEditor",
				"AppFramework",
				
				"EditorFramework",
                "EditorInteractiveToolsFramework",
                "InteractiveToolsFramework",
                "ViewportInteraction",

                "MovieScene",
                "MovieSceneTracks",
                "MovieSceneTools",
                "Sequencer",
				"LevelSequence",
				"LevelSequenceEditor",
				"SequencerScripting",

				"ControlRig",
				"ControlRigEditor"
			}
        );
	}
}
