// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DistCurveEditor : ModuleRules
{
	public DistCurveEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add("Editor/UnrealEd/Public");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "AppFramework",
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
				"LevelEditor",
				"EditorFramework",
				"UnrealEd"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"MainFrame",
				"PropertyEditor"
			}
		);
	}
}
