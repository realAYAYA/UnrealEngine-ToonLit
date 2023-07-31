// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameplayCameras : ModuleRules
{
	public GameplayCameras(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"HeadMountedDisplay",
				"MovieScene",
				"MovieSceneTracks",
				"TemplateSequence"
			}
		);
	}
}
