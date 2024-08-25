// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DiscoveryBeaconReceiver : ModuleRules
{
	public DiscoveryBeaconReceiver(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Serialization"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Engine",
				"Networking",
				"Sockets"
			}
		);
	}
}
