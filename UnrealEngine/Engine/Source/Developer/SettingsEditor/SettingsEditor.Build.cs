// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SettingsEditor : ModuleRules
{
	public SettingsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
			}
		);

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
			}
		);

		PrivateDependencyModuleNames.AddRange(
            new string[] {
				"Analytics",
                "CoreUObject",
				"DesktopPlatform",
                
				"Engine",
                "InputCore",
				"PropertyEditor",
				"SharedSettingsWidgets",
				"Slate",
				"SlateCore",
				"SourceControl",
				"DeveloperSettings"
            }
        );
	}
}
