// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class OptimusCore : ModuleRules
    {
        public OptimusCore(ReadOnlyTargetRules Target) : base(Target)
		{
			NumIncludedBytesPerUnityCPPOverride = 688128; // best unity size found from using UBT ProfileUnitySizes mode

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
					"OptimusSettings",
					"Projects",
					"RenderCore",
					"Renderer",
					"RHI",
				}
			);
        }
    }
}
