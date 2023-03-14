// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class OptimusSettings : ModuleRules
    {
        public OptimusSettings(ReadOnlyTargetRules Target) : base(Target)
        {
            PrivateIncludePaths.AddRange(
                new string[] {
					"OptimusSettings/Private",
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
					"DeveloperSettings",
					"Engine",
				}
			);
        }
    }
}
