// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LiveLinkAnimationCore : ModuleRules
{
    public LiveLinkAnimationCore(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
				"Engine",
				"LiveLinkInterface"
            }
        );
	}
}
