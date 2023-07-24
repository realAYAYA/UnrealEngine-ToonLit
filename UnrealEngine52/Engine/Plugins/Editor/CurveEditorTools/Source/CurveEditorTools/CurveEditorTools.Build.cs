// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CurveEditorTools : ModuleRules
{
	public CurveEditorTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CurveEditor",
				"Engine",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"InputCore",
				"SlateCore",
				"Slate",
				"SequencerWidgets",
				"EditorFramework",
				"UnrealEd",
				"CoreUObject",
				"AudioMixer",
				"SignalProcessing",
				"EditorStyle",
			}
		);
	}
}