// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class RemoteControlProtocolWidgets : ModuleRules
{
	public RemoteControlProtocolWidgets(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"EditorStyle",
			"EditorSubsystem",
			"EditorWidgets",
			"Engine",
			"GraphEditor",
			"InputCore",
			"Projects",
			"PropertyEditor",
			"RemoteControl",
			"RemoteControlCommon",
			"RemoteControlProtocol",
			"Slate",
			"SlateCore",
			"UnrealEd"
		});
	}
}
