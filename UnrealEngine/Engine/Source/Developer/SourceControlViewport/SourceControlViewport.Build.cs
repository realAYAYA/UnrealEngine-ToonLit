// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SourceControlViewport : ModuleRules
{
	public SourceControlViewport(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"LevelEditor",
				"Slate",
				"SlateCore",
				"SourceControl",
				"ToolMenus",
				"UnrealEd"
			}
		);
	}
}
