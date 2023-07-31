// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class KismetWidgets : ModuleRules
{
	public KismetWidgets(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core", 
				"CoreUObject",
                "InputCore",
				"Engine",
				"Slate",
				"SlateCore",
				"EditorFramework",
				"UnrealEd",
				"BlueprintGraph",
				"ContentBrowserData",
				"ClassViewer",
				"ToolWidgets",
				"EditorWidgets",
			}
		);

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
				"ContentBrowser",
                "AssetTools",
			}
		);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
			    "ContentBrowser",
                "AssetTools",
			}
		);

		// Circular references that need to be cleaned up
		CircularlyReferencedDependentModules.AddRange(
			new string[] {
				"BlueprintGraph",
			}
		);
	}
}
