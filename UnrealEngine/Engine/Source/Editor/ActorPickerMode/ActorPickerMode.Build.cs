// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ActorPickerMode : ModuleRules
{
    public ActorPickerMode(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
                "Slate",
                "SlateCore",
				"EditorFramework",
				"UnrealEd",
			}
		);
	}
}
