// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PropertyAccessNode : ModuleRules
{
	public PropertyAccessNode(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Slate",	
				"GraphEditor",
				"BlueprintGraph",
				"Engine",
				"UnrealEd",
				"SlateCore",
				"InputCore",
				"KismetWidgets",
				"KismetCompiler",
				"PropertyAccessEditor",
				"AnimGraph",
			}
		);
	}
}
