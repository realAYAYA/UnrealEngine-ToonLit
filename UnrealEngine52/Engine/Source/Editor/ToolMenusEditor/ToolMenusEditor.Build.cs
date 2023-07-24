// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ToolMenusEditor : ModuleRules
	{
		public ToolMenusEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"ToolMenus",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Slate",
					"SlateCore",
					"EditorFramework",
					"UnrealEd",
					"InputCore",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"AssetRegistry"
			});

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"AssetRegistry"
				});
		}
	}
}
