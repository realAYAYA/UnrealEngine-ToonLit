// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AnimationSettings: ModuleRules
{
	public AnimationSettings(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string [] {
				"Core",
				"CoreUObject",
				"AssetRegistry",
				"Engine", 
				"SharedSettingsWidgets"
			}
		);
	}
}
