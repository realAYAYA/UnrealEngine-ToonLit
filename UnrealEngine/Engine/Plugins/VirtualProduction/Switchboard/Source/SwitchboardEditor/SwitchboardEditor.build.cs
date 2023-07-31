// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SwitchboardEditor : ModuleRules
{
	public SwitchboardEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
				"Blutility",
				"Core",
				"CoreUObject",
				"DesktopPlatform",
				"Engine",
				
				"InputCore",
				"Json",
				"JsonUtilities",
				"MessageLog",
				"Projects",
				"PropertyEditor",
				"Settings",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"ToolWidgets",
				"UnrealEd",
			});

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDefinitions.Add("SB_LISTENER_AUTOLAUNCH=1");
			PrivateDefinitions.Add("SWITCHBOARD_SHORTCUTS=1");
		}
		else
		{
			PrivateDefinitions.Add("SB_LISTENER_AUTOLAUNCH=0");
			PrivateDefinitions.Add("SWITCHBOARD_SHORTCUTS=0");
		}
	}
}
