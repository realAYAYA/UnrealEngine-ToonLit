// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

// This module must be loaded "PreLoadingScreen" in the .uproject file, otherwise it will not hook in time!

public class PreLoadScreenMoviePlayer : ModuleRules
{
    public PreLoadScreenMoviePlayer(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateDependencyModuleNames.AddRange(
			new string[] {
                "Core",
                "RHI",
                "RenderCore",
                "MoviePlayer",
                "Slate",
                "SlateCore",
                "InputCore",
                "Projects",
                "HTTP",
                "BuildPatchServices",
                "Json",
                "Engine",
                "RenderCore",
                "ApplicationCore",
                "Analytics",
                "AnalyticsET",
                "PreLoadScreen",
                "MoviePlayer",
                "CoreUObject",
            }
		);
    }
}
