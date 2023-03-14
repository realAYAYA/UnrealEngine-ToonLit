// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ContentBrowserFileDataSource : ModuleRules
	{
		public ContentBrowserFileDataSource(ReadOnlyTargetRules Target) : base(Target)
		{
			ShortName = "CBFileDataSource";

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
					"EditorFramework",
					"AssetTools",
					"UnrealEd",
					"ToolMenus",
					"Slate",
					"SlateCore",
					"InputCore",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"DirectoryWatcher",
				}
			);
			
			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
					"DirectoryWatcher",
				}
			);
		}
	}
}
