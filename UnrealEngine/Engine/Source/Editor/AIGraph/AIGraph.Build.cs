// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AIGraph : ModuleRules
{
    public AIGraph(ReadOnlyTargetRules Target) : base(Target)
    {
		OverridePackageType = PackageOverrideType.EngineDeveloper;

		PrivateIncludePaths.AddRange(
            new string[] {
				System.IO.Path.Combine(GetModuleDirectory("AIGraph"), "Private"),
				System.IO.Path.Combine(GetModuleDirectory("GraphEditor"), "Private"),
				System.IO.Path.Combine(GetModuleDirectory("Kismet"), "Private"),
			}
        );

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
				"AssetRegistry",
				"AssetTools",
				"ContentBrowser"
			}
        );

        PrivateDependencyModuleNames.AddRange(
            new string[] {
				"Core", 
				"CoreUObject", 
				"ApplicationCore",
				"Engine", 
                "RenderCore",
                "InputCore",
				"Slate",
				"SlateCore",
                
				"EditorFramework",
				"UnrealEd", 
				"MessageLog", 
				"GraphEditor",
                "Kismet",
				"AnimGraph",
				"BlueprintGraph",
                "AIModule",
				"ClassViewer",
				"ToolMenus",
			}
        );

        DynamicallyLoadedModuleNames.AddRange(
            new string[] { 
				"AssetTools",
				"AssetRegistry",
				"ContentBrowser"
            }
        );
    }
}
