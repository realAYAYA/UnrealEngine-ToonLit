// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ContentBrowser : ModuleRules
{
	public ContentBrowser(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
				"CollectionManager",
				"EditorWidgets",
				"GameProjectGeneration",
                "MainFrame",
				"PackagesDialog",
				"SourceControl",
				"SourceControlWindows"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "AppFramework",
				"Core",
				"CoreUObject",
				"ApplicationCore",
                "InputCore",
				"Engine",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"AssetTools",
				"ContentBrowserData",
				"SourceControl",
				"SourceControlWindows",
				"WorkspaceMenuStructure",
				"EditorFramework",
				"UnrealEd",
				"EditorWidgets",
				"Projects",
				"AddContentDialog",
				"DesktopPlatform",
				"AssetRegistry",
				"AssetTagsEditor",
				"ToolMenus",
				"StatusBar",
				"ToolWidgets",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"PropertyEditor",
				"PackagesDialog",
				"CollectionManager",
				"GameProjectGeneration",
                "MainFrame"
			}
		);
		
		PublicIncludePathModuleNames.AddRange(
            new string[] {
				"AssetTools",
				"CollectionManager",
				"ContentBrowserData",
            }
        );
	}
}
