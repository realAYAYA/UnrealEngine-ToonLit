// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LiveLinkLens : ModuleRules
{
	public LiveLinkLens(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"CameraCalibrationCore",
				"CinematicCamera",
				"LiveLinkComponents",
				"LiveLinkInterface"
			}
		);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"LiveLinkMovieScene",
				"MovieScene",
				"MovieSceneTracks",
			}
		);
	}
}
