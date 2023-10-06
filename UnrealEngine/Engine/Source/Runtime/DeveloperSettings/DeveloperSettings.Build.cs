// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DeveloperSettings : ModuleRules
{
	public DeveloperSettings(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Projects"
			}
		);
	}
}
