// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AssetManagerEditor : ModuleRules
{
	public AssetManagerEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"StatsViewer",
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] { 
				"Core",
				"CoreUObject",
				"Engine",
				"TargetPlatform",
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CookMetadata",
				"Slate",
				"SlateCore",
				"ApplicationCore",
				"InputCore",
				"EditorFramework",
				"UnrealEd",
				"AssetRegistry",
				"Json",
				"CollectionManager",
				"ContentBrowser",
				"ContentBrowserData",
				"WorkspaceMenuStructure",
				"AssetDefinition",
				"AssetTools",
				"PropertyEditor",
				"GraphEditor",
				"BlueprintGraph",
				"KismetCompiler",
				"LevelEditor",
				"SandboxFile",
				"EditorWidgets",
				"TreeMap",
				"ToolMenus",
				"ToolWidgets",
				"SourceControl",
				"SourceControlWindows",
				"UncontrolledChangelists",
				"Projects",
			}
		);
	}
}
