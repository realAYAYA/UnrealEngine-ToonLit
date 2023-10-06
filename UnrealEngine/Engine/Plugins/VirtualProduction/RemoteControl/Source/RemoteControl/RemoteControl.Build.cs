// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RemoteControl : ModuleRules
{
	public RemoteControl(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"RemoteControlCommon",
				"StructUtils"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Cbor",
				"Engine",
				"RemoteControlInterception",
				"Serialization"
			}
		);

		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"AssetTools",
					"DeveloperSettings",
					"MessageLog",
					"SharedSettingsWidgets",
					"UnrealEd",
				}
			);
		}
	}
}
