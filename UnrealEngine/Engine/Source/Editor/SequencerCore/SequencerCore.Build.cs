// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SequencerCore : ModuleRules
{
	public SequencerCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"CurveEditor",
				"Engine",
				"GraphEditor",
				"InputCore",
				"Slate",
				"SlateCore",
				"TimeManagement",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"EditorFramework",
				"EditorStyle",
				"UnrealEd",
			}
		);
	}
}
