// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class LensDistortion : ModuleRules
	{
		public LensDistortion(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"RenderCore",
					"RHI",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
                {
                    "Projects",
				}
			);

		}
	}
}
