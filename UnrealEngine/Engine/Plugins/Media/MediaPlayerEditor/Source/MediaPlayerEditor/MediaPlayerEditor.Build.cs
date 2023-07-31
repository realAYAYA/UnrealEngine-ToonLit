// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MediaPlayerEditor : ModuleRules
{
	public MediaPlayerEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"MainFrame",
				"Media",
				"WorkspaceMenuStructure",
			});

		PrivateIncludePaths.AddRange(
			new string[] {
				"MediaPlayerEditor/Private",
				"MediaPlayerEditor/Private/AssetTools",
				"MediaPlayerEditor/Private/Customizations",
				"MediaPlayerEditor/Private/Factories",
				"MediaPlayerEditor/Private/Models",
				"MediaPlayerEditor/Private/Shared",
				"MediaPlayerEditor/Private/Toolkits",
				"MediaPlayerEditor/Private/Widgets",
			});

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Slate",
				"SlateCore",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AudioMixer",
				"ComponentVisualizers",
				"ContentBrowser",
				"CoreUObject",
				"ApplicationCore",
				"DesktopPlatform",
				"DesktopWidgets",	
				"EditorWidgets",
				"Engine",
				"InputCore",
				"MediaAssets",
				"MediaUtils",
				"PropertyEditor",
				"RenderCore",
				"RHI",
				"TextureEditor",
				"EditorFramework",
				"UnrealEd",
				"ToolMenus",
			});

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"Media",
				"UnrealEd",
				"WorkspaceMenuStructure",
			});
	}
}
