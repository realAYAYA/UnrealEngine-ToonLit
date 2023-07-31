// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GameplayTagsEditor : ModuleRules
	{
		public GameplayTagsEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePathModuleNames.AddRange(
				new string[] {
					"AssetTools",
					"AssetRegistry",
				}
			);


			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					// ... add private dependencies that you statically link with here ...
					"ApplicationCore",
					"Core",
					"CoreUObject",
					"Engine",
					"AssetTools",
					"AssetRegistry",
					"GameplayTags",
					"InputCore",
					"Slate",
					"SlateCore",
					"EditorStyle",
					"BlueprintGraph",
					"KismetCompiler",
					"GraphEditor",
					"ContentBrowser",
					"ContentBrowserData",
					"MainFrame",
					"EditorFramework",
					"UnrealEd",
					"SourceControl",
					"ToolMenus",
					"SettingsEditor",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"PropertyEditor",
					"Settings"
				}
			);
		}
	}
}
