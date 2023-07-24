// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ContentBrowserAliasDataSource : ModuleRules
	{
		public ContentBrowserAliasDataSource(ReadOnlyTargetRules Target) : base(Target)
		{
			ShortName = "CBADS";

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"AssetRegistry",
					"Core",
					"CoreUObject",
					"ContentBrowserAssetDataSource",
					"ContentBrowserData"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetTools",
					"Engine"
				}
			);
		}
	}
}
