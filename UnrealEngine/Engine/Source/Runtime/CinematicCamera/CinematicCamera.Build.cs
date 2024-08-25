// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CinematicCamera : ModuleRules
{
	public CinematicCamera(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"DeveloperSettings",
                "Engine",
                "MovieScene",
                "MovieSceneTracks",
                "Slate",
                "SlateCore"
			}
		);
	}
}
