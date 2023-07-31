// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class EditorSettingsViewer : ModuleRules
	{
		public EditorSettingsViewer(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"CoreUObject",
					"CurveEditor",
					"EditorFramework",
					"Engine",
					"GraphEditor",
					"InputBindingEditor",
					"MessageLog",
					"SettingsEditor",
					"Slate",
					"SlateCore",
					"UnrealEd",
                    "InternationalizationSettings",
					"BlueprintGraph",
                    "Analytics",
                    "VREditor",
					"ToolMenus"
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Settings",
				}
			);
		}
	}
}
