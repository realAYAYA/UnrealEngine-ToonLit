// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ActorSequence : ModuleRules
{
	public ActorSequence(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("ActorSequence/Private");

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
