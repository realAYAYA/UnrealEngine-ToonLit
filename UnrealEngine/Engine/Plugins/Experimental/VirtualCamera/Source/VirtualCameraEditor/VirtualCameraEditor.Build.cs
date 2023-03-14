// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VirtualCameraEditor : ModuleRules
{
	public VirtualCameraEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		DefaultBuildSettings = BuildSettingsVersion.V2;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"Settings",
				"PlacementMode",
				"VPUtilities",
				"VPUtilitiesEditor",
				"VCamCore",
				"VirtualCamera",
				"EditorWidgets",
				"CinematicCamera"
			}
		);
			
		PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"LevelEditor",
			}
		);
	}
}
