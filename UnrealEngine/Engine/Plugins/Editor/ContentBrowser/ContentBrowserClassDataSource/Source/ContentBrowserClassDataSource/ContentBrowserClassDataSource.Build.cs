// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ContentBrowserClassDataSource : ModuleRules
	{
		public ContentBrowserClassDataSource(ReadOnlyTargetRules Target) : base(Target)
		{
			ShortName = "CBClassDataSource";

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"ContentBrowserData",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"ApplicationCore",
					"AssetTools",
					"CollectionManager",
					"Engine",
					"EditorFramework",
					"UnrealEd",
					"EditorWidgets",
					"Projects",
					"GameProjectGeneration",
					"ToolMenus",
					"Slate",
					"SlateCore",
					"InputCore",
				}
			);
		}
	}
}
