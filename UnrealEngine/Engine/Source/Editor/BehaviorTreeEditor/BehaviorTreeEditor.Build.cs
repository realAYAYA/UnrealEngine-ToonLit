// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BehaviorTreeEditor : ModuleRules
{
	public BehaviorTreeEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AIGraph",
				"AIModule",
				"ApplicationCore",
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