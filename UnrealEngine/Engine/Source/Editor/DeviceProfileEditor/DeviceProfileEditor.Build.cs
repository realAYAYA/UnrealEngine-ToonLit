// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DeviceProfileEditor : ModuleRules
{
	public DeviceProfileEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.AddRange(
			new string[] {
				"Editor/UnrealEd/Public"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
				"LevelEditor",
				"EditorFramework",
				"UnrealEd",
				"WorkspaceMenuStructure",
				"PropertyEditor",
				"SourceControl",
                "TargetPlatform",
				"DesktopPlatform",
				"SharedSettingsWidgets",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"MainFrame",
			}
		);
	}
}
