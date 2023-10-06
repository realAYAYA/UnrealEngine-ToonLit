// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CSVtoSVG : ModuleRules
{
	public CSVtoSVG(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"EditorConfig",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"WorkspaceMenuStructure",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
				"EditorFramework",
				"UnrealEd",
				"ContentBrowserData",
				"Settings",
            }
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
                "AssetTools",
				"EditorWidgets",
			}
		);
	}
}
