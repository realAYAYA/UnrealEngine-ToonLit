// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TemplateSequenceEditor : ModuleRules
{
	public TemplateSequenceEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"TemplateSequence",
				"BlueprintGraph",
				"CinematicCamera",
				"Core",
				"CoreUObject",
				"EditorFramework",
				"Engine",
				"InputCore",
				"Kismet",
				"MovieScene",
				"MovieSceneTools",
				"MovieSceneTracks",
				"Sequencer",				
				"Slate",
				"SlateCore",
				"UnrealEd",
				"TimeManagement",
				"LevelSequence",
				"LevelSequenceEditor"
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"TemplateSequenceEditor/Private",
			}
		);

		var DynamicModuleNames = new string[] {
			"LevelEditor",
			"PropertyEditor",
		};

		foreach (var Name in DynamicModuleNames)
		{
			PrivateIncludePathModuleNames.Add(Name);
			DynamicallyLoadedModuleNames.Add(Name);
		}
	}
}
