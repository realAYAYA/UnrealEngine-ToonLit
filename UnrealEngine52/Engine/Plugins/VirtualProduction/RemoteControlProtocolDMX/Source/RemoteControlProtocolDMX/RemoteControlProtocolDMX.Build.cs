// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RemoteControlProtocolDMX : ModuleRules
{
	public RemoteControlProtocolDMX(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"DMXRuntime",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"DMXProtocol",
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
