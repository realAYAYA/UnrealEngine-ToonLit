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
					"AssetTools",
					"CollectionManager",
					"UnrealEd",
					"Projects",
					"GameProjectGeneration",
					"ToolMenus",
					"Slate",
					"SlateCore",
				}
			);
		}
	}
}
