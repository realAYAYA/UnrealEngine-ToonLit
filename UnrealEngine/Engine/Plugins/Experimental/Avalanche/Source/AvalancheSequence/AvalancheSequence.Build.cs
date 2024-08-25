// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AvalancheSequence : ModuleRules
{
    public AvalancheSequence(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "AvalancheTag",
                "AvalancheTransition",
                "Core",
                "CoreUObject",
                "Engine",
                "LevelSequence",
                "MovieScene",
                "MovieSceneTracks",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "AvalancheCore",
                "RemoteControl",
                "RemoteControlLogic",
                "StateTreeModule",
            });

        if (Target.Type == TargetRules.TargetType.Editor)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Sequencer",
                });
        }
    }
}