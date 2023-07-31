// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;


public class SlateScreenReader : ModuleRules
{
	public SlateScreenReader(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(
			new string[] {
			"ApplicationCore",
			"SlateCore",
			"Slate",
			"InputCore",
			"TextToSpeech",
			"ScreenReader"
		});
	}
}
