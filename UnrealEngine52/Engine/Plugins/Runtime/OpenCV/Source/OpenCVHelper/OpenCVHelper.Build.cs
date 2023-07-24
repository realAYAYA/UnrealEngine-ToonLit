// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OpenCVHelper: ModuleRules
{
	public OpenCVHelper(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine"
            }
        );

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "OpenCV",
                "Projects",
            }
        );
	}
}
