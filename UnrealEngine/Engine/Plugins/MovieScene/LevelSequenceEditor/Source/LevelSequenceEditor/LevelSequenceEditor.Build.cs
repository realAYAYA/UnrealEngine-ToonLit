// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LevelSequenceEditor : ModuleRules
{
	public LevelSequenceEditor(ReadOnlyTargetRules Target) : base(Target)
	{
        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
				"AssetTools",
				"PlacementMode",
			}
        );

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"SequencerScripting",
			}
		);
        
        PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ApplicationCore",
				"AppFramework",
                "LevelSequence",
				"BlueprintGraph",
                "CinematicCamera",
				"ClassViewer",
				"Core",
				"CoreUObject",
				"CurveEditor",
				"EditorFramework",
				"EditorSubsystem",
				"Engine",
                "InputCore",
                "Kismet",
                "KismetCompiler",
				"LevelEditor",
				"MovieScene",
                "MovieSceneTools",
				"MovieSceneTracks",
                "PropertyEditor",
				"Sequencer",
                "Slate",
                "SlateCore",
				"SceneOutliner",
                "UnrealEd",
                "VREditor",
				"TimeManagement",
				"ToolMenus",
				"ToolWidgets",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools",
                "MovieSceneTools",
				"SceneOutliner",
				"PlacementMode",
                "Settings",
                "MovieSceneCaptureDialog",
			}
		);

        PrivateIncludePaths.AddRange(
            new string[] {
            	"LevelSequenceEditor/Public",
				"LevelSequenceEditor/Private",
				"LevelSequenceEditor/Private/AssetTools",
				"LevelSequenceEditor/Private/Factories",
                "LevelSequenceEditor/Private/Misc",
				"LevelSequenceEditor/Private/Styles",
			}
        );
	}
}
