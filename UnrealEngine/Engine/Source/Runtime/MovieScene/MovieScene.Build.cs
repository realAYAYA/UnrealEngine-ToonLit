// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MovieScene : ModuleRules
{
	public MovieScene(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "TargetPlatform",
				"UniversalObjectLocator"
			}
        );

        PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "InputCore",
                "Engine",
				"TimeManagement",
				"UniversalObjectLocator"
			}
		);
		SetupIrisSupport(Target);
	}
}
