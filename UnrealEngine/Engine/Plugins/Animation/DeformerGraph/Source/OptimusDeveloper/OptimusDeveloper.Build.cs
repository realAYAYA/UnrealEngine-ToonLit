// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class OptimusDeveloper : ModuleRules
    {
        public OptimusDeveloper(ReadOnlyTargetRules Target) : base(Target)
        {
	        PrivateIncludePaths.AddRange(
                new string[] {
					"OptimusDeveloper/Private",
				}
            );

            PublicDependencyModuleNames.AddRange(
				new string[]
				{
				}
	        );

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
					"CoreUObject",
					"Engine",
					"ComputeFramework",
					"OptimusCore",
					"RenderCore",
				}
			);
        }
    }
}
