// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ReplayTracks : ModuleRules
{
	public ReplayTracks(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("ReplayTracks/Private");

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
