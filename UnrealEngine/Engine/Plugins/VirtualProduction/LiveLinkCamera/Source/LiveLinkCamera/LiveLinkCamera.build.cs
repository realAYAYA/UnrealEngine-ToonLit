// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LiveLinkCamera : ModuleRules
{
	public LiveLinkCamera(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"CameraCalibrationCore",
				"CinematicCamera",
				"Core",
				"CoreUObject",
				"Engine",
				"LiveLinkInterface",
				"LiveLink",
				"LiveLinkComponents",
			}
		);
	}
}
