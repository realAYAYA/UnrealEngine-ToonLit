// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RemoteControlWebInterface : ModuleRules
{
	public RemoteControlWebInterface(ReadOnlyTargetRules Target) : base(Target)
	{
		ShortName = "RCWebIntf";

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"Projects",
				"RemoteControl",
				"Sockets",
				"WebRemoteControl",
				"WebSocketNetworking",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] 
			{
				"RemoteControlCommon",
				"Serialization",
			}
		);


		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] 
				{
					"DeveloperSettings",
					"EditorStyle",
					"EditorWidgets",
					"PropertyEditor",
					"RemoteControl",
					"RemoteControlUI",
					"Settings",
					"UnrealEd",
					"ToolWidgets",
				}
			);
		}
	}
}
