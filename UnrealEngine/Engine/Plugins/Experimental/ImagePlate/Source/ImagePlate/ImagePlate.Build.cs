// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ImagePlate : ModuleRules
	{
		public ImagePlate(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"ImageCore",
					"CoreUObject",
					"Engine",
					"ImageWrapper",
					"MediaAssets",
					"RenderCore",
					"RHI",
					"TimeManagement",
                    "SlateCore",
                }
			);

			PrivateIncludePaths.AddRange(
				new string[] {
				}
			);
		}
	}
}
