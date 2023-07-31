// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UMGEditor : ModuleRules
{
	public UMGEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		OverridePackageType = PackageOverrideType.EngineDeveloper;

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"UMG",
			});

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Sequencer",
            });

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ClassViewer",
				"Core",
				"CoreUObject",
				"ContentBrowser",
				"ContentBrowserData",
				"ApplicationCore",
				"InputCore",
				"Engine",
				"AssetTools",
				"EditorConfig",
				"EditorSubsystem",
				"EditorFramework",
				"InteractiveToolsFramework",
				"UnrealEd", // for Asset Editor Subsystem
				"KismetWidgets",
				"EditorWidgets",
				"KismetCompiler",
				"BlueprintGraph",
				"GraphEditor",
				"Kismet",  // for FWorkflowCentricApplication
				"PropertyPath",
				"PropertyEditor",
				"UMG",
				"EditorStyle",
				"Slate",
				"SlateCore",
				"SlateRHIRenderer",
				"StatusBar",
				"MessageLog",
				"MovieScene",
				"MovieSceneTools",
                "MovieSceneTracks",
				"DetailCustomizations",
                "Settings",
				"RenderCore",
                "TargetPlatform",
				"TimeManagement",
				"GameProjectGeneration",
				"PropertyPath",
				"ToolMenus",
				"SlateReflector",
				"DeveloperSettings",
				"ImageWrapper",
				"ToolWidgets",
				"WorkspaceMenuStructure"
			}
			);
	}
}
