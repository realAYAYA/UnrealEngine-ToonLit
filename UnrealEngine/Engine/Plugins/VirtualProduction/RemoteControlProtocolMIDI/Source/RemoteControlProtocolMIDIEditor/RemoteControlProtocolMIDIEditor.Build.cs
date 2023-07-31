// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class RemoteControlProtocolMIDIEditor : ModuleRules
{
	public RemoteControlProtocolMIDIEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"MIDIDevice",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"EditorStyle",
				"EditorWidgets",
				"InputCore",
				"PropertyEditor",
				"RemoteControl",
				"RemoteControlProtocol",
				"RemoteControlProtocolMIDI",
				"Slate",
				"SlateCore",
			}
		);
	}
}
