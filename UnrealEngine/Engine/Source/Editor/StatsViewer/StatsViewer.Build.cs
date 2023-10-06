// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StatsViewer : ModuleRules
{
	public StatsViewer(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Slate",
				"SlateCore",
				"RHI",
				"EditorFramework",
				"UnrealEd",
				"Landscape"
			}
		);

        PrivateIncludePathModuleNames.AddRange(
			new string[] {
                "PropertyEditor",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"PropertyEditor"
			}
		);
	}
}
