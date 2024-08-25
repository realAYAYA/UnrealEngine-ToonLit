// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AvalanchePropertyAnimator : ModuleRules
{
    public AvalanchePropertyAnimator(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
	            "AvalancheSequence",
	            "AvalancheText",
	            "Core",
                "CoreUObject",
                "Engine",
                "MovieScene",
                "PropertyAnimator",
                "PropertyAnimatorCore",
                "Slate",
                "SlateCore",
                "Text3D"
            }
        );

        if (Target.Type == TargetRules.TargetType.Editor)
        {
	        PrivateDependencyModuleNames.Add("Sequencer");
        }
    }
}