// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class MobilePatchingUtils : ModuleRules
    {
        public MobilePatchingUtils(ReadOnlyTargetRules Target) : base(Target)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
            });

            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "PakFile",
                "HTTP",
                "BuildPatchServices"
            });
        }
    }
}
