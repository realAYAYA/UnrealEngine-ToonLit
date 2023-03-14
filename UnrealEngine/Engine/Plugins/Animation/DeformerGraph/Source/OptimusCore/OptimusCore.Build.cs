// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class OptimusCore : ModuleRules
    {
        public OptimusCore(ReadOnlyTargetRules Target) : base(Target)
        {
            PrivateIncludePaths.AddRange(
                new string[] {
                    "OptimusCore/Private",
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
					"ComputeFramework",
					"Core",
					"CoreUObject",
					"Engine",
					"Projects",
					"RenderCore",
					"Renderer",
					"RHI",
				}
			);
        }
    }
}
