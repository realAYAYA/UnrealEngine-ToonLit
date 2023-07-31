// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System;
using System.IO;

public class WebMMoviePlayer : ModuleRules
{
	public WebMMoviePlayer(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"MoviePlayer",
				"RenderCore",
				"RHI",
				"SlateCore",
				"Slate",
				"MediaUtils",
				"WebMMedia",
			});

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PrivateDependencyModuleNames.Add("SDL2");

			PrivateIncludePaths.Add("WebMMoviePlayer/Private/Audio/Unix");
		}
		else
		{
			PrivateIncludePaths.Add("WebMMoviePlayer/Private/Audio/Null");
		}
		PrivateIncludePaths.Add("WebMMoviePlayer/Private/Audio");

		// Some Linux architectures don't have the libs built yet
		bool bHaveWebMlibs = (!Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) || Target.Architecture.StartsWith("x86_64"));
		PublicDefinitions.Add("WITH_WEBM_LIBS=" + (bHaveWebMlibs ? "1" : "0"));
	}
}
