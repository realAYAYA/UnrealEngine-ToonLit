// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ActorSequenceEditor : ModuleRules
{
	public ActorSequenceEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ActorSequence",
				"BlueprintGraph",
				"Core",
				"CoreUObject",
				"EditorFramework",
				"Engine",
				"InputCore",
				"Kismet",
				"MovieScene",
				"MovieSceneTools",
				"Sequencer",
				"Slate",
				"SlateCore",
				"SubobjectEditor",
				"SubobjectDataInterface",
				"UnrealEd",
				"TimeManagement"
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
