// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TurnkeySupport : ModuleRules
{
	public TurnkeySupport(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"DeveloperToolSettings",
				"EngineSettings",
				"InputCore",
				"RHI",
				"RenderCore",
				"Slate",
				"SlateCore",
				"TargetPlatform",
				"DesktopPlatform",
				"WorkspaceMenuStructure",
				"MessageLog",
 				"Projects",
 				"ToolMenus",
 				"LauncherServices",
				"SourceControl",
				"TurnkeyIO",
				"Analytics",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"DerivedDataCache",
			}
		);

		if (Target.bCompileAgainstEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
				"UnrealEd",
				"UATHelper",
 				"SettingsEditor",
				"Zen",
				}
			);
			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"GameProjectGeneration",
					"ProjectTargetPlatformEditor",
					"LevelEditor",
					"Settings",
	 				"MainFrame",
				}
			);
		}
	}
}
