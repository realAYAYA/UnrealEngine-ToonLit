// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LiveLinkCameraEditor : ModuleRules
{
	public LiveLinkCameraEditor(ReadOnlyTargetRules Target) : base(Target)
	{		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CameraCalibrationCore",
				"Core",
				"CoreUObject",
				"DetailCustomizations",
				
				"LiveLinkInterface",
				"LiveLinkCamera",
				"LiveLinkComponents",
				"PropertyEditor",
				"Slate",
				"SlateCore"
			}
		);
	}
}
