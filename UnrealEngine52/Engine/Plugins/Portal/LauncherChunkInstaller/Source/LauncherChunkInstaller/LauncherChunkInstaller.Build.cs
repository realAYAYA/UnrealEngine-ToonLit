// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LauncherChunkInstaller : ModuleRules
{
    public LauncherChunkInstaller(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[] 
            {
                "Core",
                "PakFile",
            }
        );

        if (Target.Platform != UnrealTargetPlatform.Linux && Target.Platform != UnrealTargetPlatform.Win64 && Target.Platform != UnrealTargetPlatform.Mac)
        {
            PrecompileForTargets = PrecompileTargetsType.None;
        }
    }
}