// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RemoteControlProtocolMIDI : ModuleRules
{
	public RemoteControlProtocolMIDI(ReadOnlyTargetRules Target) : base(Target)
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
				"RemoteControl",
				"RemoteControlProtocol",
			}
		);
		
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"InputCore",
					"RemoteControlProtocolWidgets",
				}
			);
		}
	}
}
