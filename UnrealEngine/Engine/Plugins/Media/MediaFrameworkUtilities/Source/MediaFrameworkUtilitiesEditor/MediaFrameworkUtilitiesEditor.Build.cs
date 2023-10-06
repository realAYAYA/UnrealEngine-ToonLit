// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MediaFrameworkUtilitiesEditor : ModuleRules
	{
		public MediaFrameworkUtilitiesEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetRegistry",
					"AssetTools",
					"ClassViewer",
					"Core",
					"CoreUObject",
					"EditorFramework",
					"EditorStyle",
					"EditorWidgets",
					"Engine",
					"RenderCore",
					"InputCore",
					"LevelEditor",
					"MainFrame",
					"MaterialEditor",
					"MediaAssets",
					"MediaFrameworkUtilities",
					"MediaIOCore",
                    "MediaPlayerEditor",
                    "MediaUtils",
					"PlacementMode",
					"PropertyEditor",
					"SharedSettingsWidgets",
					"Slate",
					"SlateCore",
					"TimeManagement",
					"ToolMenus",
					"UnrealEd",
					"WorkspaceMenuStructure",
				});
		}
	}
}
