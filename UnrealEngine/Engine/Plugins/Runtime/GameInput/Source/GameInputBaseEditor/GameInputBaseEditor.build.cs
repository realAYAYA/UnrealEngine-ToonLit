// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameInputBaseEditor : ModuleRules
{
	public GameInputBaseEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"GameInputBase",
				"Settings"
			}
		);
	}
}
