// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WebRemoteControl : ModuleRules
{
	public WebRemoteControl(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"HTTPServer",
				"Serialization"
			}
		);

        PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
				"HTTP",
				"Networking",
				"RemoteControl",
				"RemoteControlCommon",
				"RemoteControlLogic",
				"Sockets",
				"WebSocketNetworking"
			}
        );

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"DeveloperSettings",
					"Engine",
					"ImageWrapper",
					"RemoteControlUI",
					"Settings",
					"SharedSettingsWidgets",
					"Slate",
					"SlateCore",
					"SourceControl",
					"UnrealEd",
				}
			);
		}
	}
}
