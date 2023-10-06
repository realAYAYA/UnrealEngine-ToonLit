// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OpenCVLensCalibration : ModuleRules
{
	public OpenCVLensCalibration(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "OpenCVHelper",
                "OpenCV",
				"OpenCVLensDistortion",
            }
        );
	}
}
