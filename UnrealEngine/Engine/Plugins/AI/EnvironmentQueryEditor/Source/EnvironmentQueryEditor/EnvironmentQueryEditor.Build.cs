// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EnvironmentQueryEditor : ModuleRules
{
	public EnvironmentQueryEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AIGraph",
				"AIModule",
				"AssetDefinition",
				"Core",
				"CoreUObject",
				"Engine",
				"GraphEditor",
				"InputCore",
				"KismetWidgets",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"UnrealEd",
			}
		);
	}
}