// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MacPlatformEditor : ModuleRules
{
	public MacPlatformEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"InputCore",
				"DesktopPlatform",
				"Engine",
				"MainFrame",
				"Slate",
				"SlateCore",
				
				"PropertyEditor",
				"SharedSettingsWidgets",
				"SourceControl",
				"MacTargetPlatform",
				"TargetPlatform",
				"MaterialShaderQualitySettings",
				"RenderCore",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"GameProjectGeneration",
				"Settings",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"GameProjectGeneration",
				}
		);
	}
}
