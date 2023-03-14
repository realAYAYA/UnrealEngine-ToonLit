// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MixedRealityCaptureFramework : ModuleRules
{
	public MixedRealityCaptureFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"MediaAssets"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"Media",
				"HeadMountedDisplay",
				"InputCore",
                "MediaUtils",
				"RenderCore",
				"OpenCVLensDistortion",
				"OpenCVHelper",
			}
		);

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.Add("EditorFramework");
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
