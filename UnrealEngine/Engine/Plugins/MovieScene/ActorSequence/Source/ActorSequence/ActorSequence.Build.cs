// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ActorSequence : ModuleRules
{
	public ActorSequence(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"MovieScene",
				"MovieSceneTracks",
				"TimeManagement",
			}
		);
	}
}
