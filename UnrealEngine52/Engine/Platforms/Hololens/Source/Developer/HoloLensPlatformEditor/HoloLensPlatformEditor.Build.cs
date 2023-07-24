// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HoloLensPlatformEditor : ModuleRules
{
	public HoloLensPlatformEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Settings",
				"DeveloperToolSettings",
				"DesktopPlatform",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"PropertyEditor",
				"SharedSettingsWidgets",
				"AppFramework",
				"DesktopWidgets",
				"EditorFramework",
				"UnrealEd",
				"SourceControl",
				"WindowsTargetPlatform", // For ECompilerVersion
				"HoloLensTargetPlatform",
				"EngineSettings",
				"Projects",
				"gltfToolkit",
				"AudioSettingsEditor",
			}
		);

		PublicSystemLibraries.Add("crypt32.lib");
	}
}
