// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class ComputeFramework : ModuleRules
    {
        public ComputeFramework(ReadOnlyTargetRules Target) : base(Target)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
					"CoreUObject",
					"Engine",
					"Projects",
					"RenderCore",
					"Renderer",
					"RHI",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"DerivedDataCache",
				}
			);
		}
	}
}
