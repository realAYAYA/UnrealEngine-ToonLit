// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AnimationCore : ModuleRules
{
    public AnimationCore(ReadOnlyTargetRules Target) : base(Target)
    {
		PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"Engine"
			}
		);

		PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
            }
        );
    }
}
