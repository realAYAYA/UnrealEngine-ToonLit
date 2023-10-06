// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LocalizationDashboard : ModuleRules
{
	public LocalizationDashboard(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "CoreUObject",
                "PropertyEditor",
                "Localization"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
				"Slate",
				"SlateCore",
				"InputCore",
				"EditorFramework",
                "UnrealEd",
				"DesktopPlatform",
                "TranslationEditor",
                "MainFrame",
                "SourceControl",
                "SharedSettingsWidgets",
				"LocalizationCommandletExecution",
				"LocalizationService",
				"InternationalizationSettings",
				"ToolMenus",
				"WorkspaceMenuStructure",
			}
		);

        CircularlyReferencedDependentModules.AddRange(
           new string[] {
                "LocalizationService",
				"MainFrame",
				"TranslationEditor"
            }
           );
	}
}
