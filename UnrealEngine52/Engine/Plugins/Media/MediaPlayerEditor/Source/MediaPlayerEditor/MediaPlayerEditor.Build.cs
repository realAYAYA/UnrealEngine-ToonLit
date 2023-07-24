// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
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
				"Renderer",
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


		var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
		PrivateIncludePaths.Add(Path.Combine(EngineDir, "Source", "Runtime", "Renderer", "Private"));

	}
}
