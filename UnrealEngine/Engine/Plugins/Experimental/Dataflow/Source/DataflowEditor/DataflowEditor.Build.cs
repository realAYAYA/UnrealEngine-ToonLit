// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DataflowEditor : ModuleRules
	{
        public DataflowEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
				    "Slate",
				    "SlateCore",
				    "Engine",
					"EditorFramework",
					"UnrealEd",
					"Projects",
					"PropertyEditor",
				    "RenderCore",
				    "RHI",
				    "AssetTools",
				    "AssetRegistry",
				    "SceneOutliner",
					"EditorStyle",
					"AssetTools",
					"ToolMenus",
					"LevelEditor",
					"InputCore",
					"AdvancedPreviewScene",
					"GraphEditor",
					"DataflowCore",
					"DataflowEngine",
					"Slate",
					"DeveloperSettings",
				}
			);
		}
	}
}
