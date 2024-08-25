// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AvalancheMRQEditor : ModuleRules
{
    public AvalancheMRQEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Avalanche",
                "AvalancheMRQ",
                "AvalancheMedia",
                "AvalancheMediaEditor",
                "AvalancheSequence",
                "AvalancheSequencer",
                "Core",
                "CoreUObject",
                "DeveloperSettings",
                "Engine",
                "MovieRenderPipelineCore",
                "MovieRenderPipelineEditor",
                "MovieRenderPipelineRenderPasses",
                "RemoteControl",
                "Slate",
                "SlateCore",
                "UnrealEd", 
            }
        );
    }
}