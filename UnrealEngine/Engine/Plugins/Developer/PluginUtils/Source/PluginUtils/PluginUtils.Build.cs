// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PluginUtils : ModuleRules
	{
		public PluginUtils(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Projects",
					"Core",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"UnrealEd",
					"GameProjectGeneration",
					"DesktopPlatform",
					"AssetRegistry",
					"AssetTools",
					"CoreUObject",
					"SourceControl"
				}
			);
		}
	}
}
