// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Kismet : ModuleRules
{
	public Kismet(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				"Editor/GraphEditor/Private",
				"Editor/SceneOutliner/Private",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] { 
				"AssetRegistry", 
				"AssetTools",
                "BlueprintRuntime",
                "ClassViewer",
				"Analytics",
                "DerivedDataCache",
                "LevelEditor",
				"GameProjectGeneration",
				"SourceCodeAccess",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "AppFramework",
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"EditorWidgets",
				"Engine",
				"Json",
				"Merge",
				"MessageLog",
				"EditorFramework",
				"UnrealEd",
				"GraphEditor",
				"KismetWidgets",
				"KismetCompiler",
				"BlueprintGraph",
				"BlueprintEditorLibrary",
				"AnimGraph",
				"PropertyEditor",
				"SourceControl",
                "SharedSettingsWidgets",
                "InputCore",
				"EngineSettings",
                "Projects",
                "JsonUtilities",
				"DesktopPlatform",
				"HotReload",
                "UMGEditor",
                "UMG", // for SBlueprintDiff
                "WorkspaceMenuStructure",
				"DeveloperSettings",
				"ToolMenus",
				"SubobjectEditor",
				"SubobjectDataInterface",
				"ToolWidgets",
            }
			);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "BlueprintRuntime",
                "ClassViewer",
				"Documentation",
				"GameProjectGeneration",
			}
            );

		// Circular references that need to be cleaned up
		CircularlyReferencedDependentModules.AddRange(
			new string[] {
				"BlueprintGraph",
				"UMGEditor",
				"Merge"
            }
        ); 
	}
}
