// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class RemoteControlUI : ModuleRules
{
	public RemoteControlUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(new string[] {
			Path.Combine(GetModuleDirectory("PropertyEditor"), "Private"),
		});

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
