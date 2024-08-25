// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnimationDataController : ModuleRules
	{
        public AnimationDataController(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject"
                }
             );

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Engine",
					"MovieScene"
				}
			);
			
			if (Target.bCompileAgainstEditor)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{													
						"UnrealEd",
						"CurveEditor"
					}
				);
			}
		}
	}
}
