// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PropertyAnimatorEditor : ModuleRules
{
    public PropertyAnimatorEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateIncludePaths.AddRange(
            new string[] 
            {
                System.IO.Path.Combine(GetModuleDirectory("MovieSceneTools"), "Private"),
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
	            "Core",
                "CoreUObject",
                "Engine",
                "MovieScene",
                "MovieSceneTools",
				"Projects",
                "PropertyAnimator",
                "PropertyAnimatorCore",
                "PropertyAnimatorCoreEditor",
                "Sequencer",
                "Slate",
                "SlateCore",
                "ToolMenus",
                "UnrealEd"
            }
        );

        ShortName = "PropAnimEd";
    }
}