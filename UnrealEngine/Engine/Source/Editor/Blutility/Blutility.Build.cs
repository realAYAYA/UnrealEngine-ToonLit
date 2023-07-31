// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Blutility : ModuleRules
{
	public Blutility(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("AssetTools");

        PublicDependencyModuleNames.AddRange(new string[] {
			"EditorSubsystem",
			"MainFrame"
		});

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
				"Kismet",
				"AssetRegistry",
				"AssetTools",
				"WorkspaceMenuStructure",
				"ContentBrowser",
				"ContentBrowserData",
				"ClassViewer",
				"CollectionManager",
                "PropertyEditor",
                "BlueprintGraph",
				"UMG",
                "UMGEditor",
                "KismetCompiler",
				"ToolMenus",
				"RHI",
				"RenderCore",
				"ImageWrapper",
				"ImageWriteQueue"
			}
			);
	}
}
