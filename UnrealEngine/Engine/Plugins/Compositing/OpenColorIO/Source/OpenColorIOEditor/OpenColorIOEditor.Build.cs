// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class OpenColorIOEditor : ModuleRules
	{
		public OpenColorIOEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"DesktopWidgets",
					"EditorFramework",
					"Engine",
					"LevelEditor",
					"OpenColorIO",
					"OpenColorIOLib",
					"Projects",
					"PropertyEditor",
					"Renderer",
					"RenderCore",
					"RHI",
					"Settings",
					"Slate",
					"SlateCore",
					"ToolMenus",
					"UnrealEd",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
				});

			PrivateIncludePaths.AddRange(
				new string[] {
				});
		}
	}
}
