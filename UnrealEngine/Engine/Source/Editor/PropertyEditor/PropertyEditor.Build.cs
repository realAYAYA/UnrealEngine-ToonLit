// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PropertyEditor : ModuleRules
{
	public PropertyEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"EditorFramework",
				"UnrealEd",
                "ActorPickerMode",
                "SceneDepthPickerMode",
				"EditorConfig",
			}
		);
		
        PublicIncludePathModuleNames.AddRange(
            new string[] {
				"EditorFramework",
			}
        );

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"MainFrame",
                "AssetRegistry",
                "AssetTools",
				"ClassViewer",
				"StructViewer",
				"ContentBrowser",
				"ConfigEditor",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "AppFramework",
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"EditorStyle",
				"Engine",
				"InputCore",
				"Slate",
				"SlateCore",
				"EditorWidgets",
				"Documentation",
                "RHI",
				"ConfigEditor",
                "SceneOutliner",
				"DesktopPlatform",
				"PropertyPath",
				"ToolWidgets",
			}
        );

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
                "AssetRegistry",
                "AssetTools",
				"ClassViewer",
				"StructViewer",
				"ContentBrowser",
				"MainFrame",
			}
		);
	}
}
