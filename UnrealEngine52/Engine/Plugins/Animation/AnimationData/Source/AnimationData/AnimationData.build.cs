// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AnimationData : ModuleRules
{
    public AnimationData(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "MovieScene",
                "MovieSceneTracks",
                "Sequencer", 
                "AnimGraph",
                "ControlRig",
                "ControlRigDeveloper",
                "UnrealEd",
                "AnimationDataController",
                "AnimationCore",
                "AnimationBlueprintLibrary",
                "TimeManagement"
            });

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "AnimGraphRuntime",   
                "MovieSceneTools",
            }
        );
    }
}
