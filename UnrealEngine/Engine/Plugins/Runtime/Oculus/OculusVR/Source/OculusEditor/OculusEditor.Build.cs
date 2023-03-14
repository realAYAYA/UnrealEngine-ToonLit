// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OculusEditor : ModuleRules
{
	public OculusEditor(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Projects",
				"InputCore",
				"EditorFramework",
				"UnrealEd",
				"LevelEditor",
				"CoreUObject",
				"Engine",
				"EngineSettings",
				"AndroidRuntimeSettings",
				"Slate",
				"SlateCore",
				
				"Core",
				"OculusHMD",
				"OVRPlugin",
				"HTTP",
				"DesktopPlatform",
                "LauncherServices",
                "GameProjectGeneration",
				"SharedSettingsWidgets",
            }
			);

		PrivateIncludePaths.AddRange(
				new string[] {
					// Relative to Engine\Plugins\Runtime\Oculus\OculusVR\Source
					"OculusEditor/Private",
					"OculusHMD/Private",
				});

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
            }
            );
	}
}
