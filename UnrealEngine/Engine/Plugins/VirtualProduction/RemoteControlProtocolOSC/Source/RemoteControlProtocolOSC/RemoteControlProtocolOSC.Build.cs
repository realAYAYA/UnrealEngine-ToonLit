// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RemoteControlProtocolOSC : ModuleRules
{
	public RemoteControlProtocolOSC(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Networking",
				"OSC",
				"RemoteControl",
				"RemoteControlProtocol"
			}
		);

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"InputCore",
				    "RemoteControlProtocolWidgets",
					"Settings",
				}
			);
		}
	}
}
