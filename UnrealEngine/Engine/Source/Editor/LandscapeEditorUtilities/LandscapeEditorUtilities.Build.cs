// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LandscapeEditorUtilities : ModuleRules
{
	public LandscapeEditorUtilities(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"Landscape",
			}
		);
	}
}
