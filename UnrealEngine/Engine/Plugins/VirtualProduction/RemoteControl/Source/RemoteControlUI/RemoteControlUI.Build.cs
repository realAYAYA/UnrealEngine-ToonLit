// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RemoteControlUI : ModuleRules
{
	public RemoteControlUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {}
		);

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
				"DeveloperSettings",
				"EditorWidgets",
				"EditorStyle",
				"Engine",
				"GraphEditor",
				"HotReload",
				"InputCore",
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
				"Slate",
				"SlateCore",
				"StructUtils",
				"ToolMenus",
				"ToolWidgets",
				"UnrealEd"
			}
		);
    }
}
