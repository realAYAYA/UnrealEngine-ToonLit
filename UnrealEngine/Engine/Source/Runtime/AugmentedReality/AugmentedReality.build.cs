// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class AugmentedReality : ModuleRules
    {
        public AugmentedReality(ReadOnlyTargetRules Target) : base(Target)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "EngineSettings",
                    "RenderCore",
                    "RHI"
                }
           );
		   
		   if (Target.bBuildEditor)
		   {
			  PrivateDependencyModuleNames.Add("UnrealEd");
		   }

            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "MRMesh"
                }
            );

            PublicIncludePathModuleNames.AddRange(
                new string[]
                {
                    "HeadMountedDisplay",
                }
           );

        }
    }
}
