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
					"CoreUObject",
					"Engine"
                }
             );

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{					
					"MovieScene"
				}
			);
			
			if (Target.Type == TargetType.Editor)
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
