// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CurveEditor : ModuleRules
{
	public CurveEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "ApplicationCore",
                "AppFramework",
                "Core",
				"CoreUObject",
				"EditorFramework",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
				"TimeManagement",
				"UnrealEd",
				"SequencerWidgets",
				"ToolMenus",
			}
		);

        PublicDependencyModuleNames.Add("SequencerWidgets");
	}
}
