// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class RemoteControlUI : ModuleRules
{
	public RemoteControlUI(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ApplicationCore",
				"AssetRegistry",
				"AssetTools",
				"BlueprintGraph",
				"ClassViewer",
				"Core",
				"CoreUObject",
				"ContentBrowser",
				"DesktopWidgets",
				"DeveloperSettings",
				"EditorWidgets",
				"EditorStyle",
				"Engine",
				"GraphEditor",
				"HotReload",
				"InputCore",
				"LevelEditor",
				"MainFrame",
				"MessageLog",
				"Projects",
				"PropertyEditor",
				"RemoteControl",
				"RemoteControlCommon",
				"RemoteControlLogic",				
				"RemoteControlProtocol",
				"RemoteControlProtocolWidgets",
				"SceneOutliner",
				"Serialization",
				"Slate",
				"SlateCore",
				"StructUtils",
				"ToolMenus",
				"ToolWidgets",
				"TypedElementFramework",
				"TypedElementRuntime",
				"UnrealEd"
			}
		);
    }
}
