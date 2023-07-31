// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RemoteControlInterception : ModuleRules
{
	public RemoteControlInterception(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Cbor",
				"Core",
				"CoreUObject",
				"Engine",
				"Serialization",
			});
	}
}
