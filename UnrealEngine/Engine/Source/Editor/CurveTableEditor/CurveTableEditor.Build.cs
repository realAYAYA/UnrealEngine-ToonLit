// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CurveTableEditor : ModuleRules
{
	public CurveTableEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.Add("LevelEditor");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core", 
				"CoreUObject", 
                "CurveEditor",
				"EditorFramework",
				"Engine", 
				"InputCore",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"EditorWidgets",
				"ToolWidgets",
			}
		);

		DynamicallyLoadedModuleNames.Add("WorkspaceMenuStructure");
	}
}
