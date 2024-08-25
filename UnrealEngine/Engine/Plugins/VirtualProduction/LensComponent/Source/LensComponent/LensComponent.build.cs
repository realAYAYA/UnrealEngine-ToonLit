// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class LensComponent : ModuleRules
	{
		public LensComponent(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"LiveLinkComponents",
					"LiveLinkInterface",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"CameraCalibrationCore",
					"CinematicCamera",
					"Core",
					"CoreUObject",
					"Engine",
				}
			);
		}
	}
}
