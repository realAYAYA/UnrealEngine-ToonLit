// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TextureEditor : ModuleRules
{
	public TextureEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"MainFrame",
				"WorkspaceMenuStructure"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "ImageCore",
                "InputCore",
				"Engine",
				"RenderCore",
				"RHI",
				"Slate",
				"SlateCore",
				"EditorFramework",
				"UnrealEd",
                "PropertyEditor",
				"EditorWidgets",
				"MediaAssets",
				"DerivedDataCache",
				"DeveloperToolSettings"
			}
		);
	}
}
