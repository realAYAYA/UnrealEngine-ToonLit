// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class PerformanceMonitor : ModuleRules
    {
        public PerformanceMonitor(ReadOnlyTargetRules Target) : base(Target)
        {
            PublicDependencyModuleNames.AddRange
            (
                new string[] {
				"Core",
                "Engine",
                "InputCore",
				"RHI",
				"RenderCore",
			}
            );

            if (Target.bBuildEditor)
            {
                PrivateDependencyModuleNames.AddRange(
                    new string[] {
					
				}
                );
            }

        }
    }
}