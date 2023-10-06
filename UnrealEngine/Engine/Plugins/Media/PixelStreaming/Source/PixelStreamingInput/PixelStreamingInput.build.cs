// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PixelStreamingInput : ModuleRules
	{
		public PixelStreamingInput(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"InputDevice",
					"Json",
					"SlateCore",
					"Slate",
					"DeveloperSettings",
					"HeadMountedDisplay",
					"XRBase",
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"ApplicationCore",
					"PixelStreamingHMD",
					"InputCore",
					"Core"
				}
			);
		}
	}
}