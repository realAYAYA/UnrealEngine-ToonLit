// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ComponentVisualizers : ModuleRules
{
	public ComponentVisualizers(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "InputCore",
				"Engine",
				"Slate",
                "SlateCore",
				"EditorFramework",
				"UnrealEd",
				"LevelEditor",
                "PropertyEditor",
				"AIModule",
				"ViewportInteraction"
			}
		);
	}
}
