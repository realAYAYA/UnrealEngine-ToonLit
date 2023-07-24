// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NewLevelDialog : ModuleRules
{
	public NewLevelDialog(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "InputCore",
				"RenderCore",
				"RHI",
				"Engine",
				"Slate",
				"SlateCore",
				"EditorFramework",
				"UnrealEd",
				"ToolWidgets",
			}
		);
	}
}
