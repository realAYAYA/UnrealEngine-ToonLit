// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ChangelistReview : ModuleRules
{
	public ChangelistReview(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"CoreUObject",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"SlateCore",
				"Slate",
				"EditorFramework",
				"UnrealEd",
				"EditorStyle",
				"ToolMenus",
				"SourceControl",
				"InputCore"
			}
		);
	}
}