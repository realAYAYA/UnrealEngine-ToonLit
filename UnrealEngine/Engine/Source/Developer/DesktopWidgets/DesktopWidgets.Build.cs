// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DesktopWidgets : ModuleRules
{
	public DesktopWidgets(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Slate",
				"SlateCore",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "DesktopPlatform",
                "InputCore",
			}
		);
	}
}
