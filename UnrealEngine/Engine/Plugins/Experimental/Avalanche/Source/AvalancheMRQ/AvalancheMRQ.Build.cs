// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AvalancheMRQ : ModuleRules
{
    public AvalancheMRQ(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "AvalancheMedia",
                "Core", 
                "MovieRenderPipelineCore",
                "RemoteControl",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Avalanche",
                "AvalancheRemoteControl",
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
            }
        );
    }
}