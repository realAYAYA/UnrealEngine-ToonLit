// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TimeManagement : ModuleRules
{
	public TimeManagement(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"SlateCore",
				"Slate",
				"Engine",
			}
		);
    }
}
