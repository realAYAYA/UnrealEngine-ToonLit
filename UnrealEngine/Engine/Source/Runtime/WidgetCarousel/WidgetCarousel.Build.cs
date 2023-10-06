// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WidgetCarousel : ModuleRules
{
	public WidgetCarousel(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"ApplicationCore",
				"Slate",
				"SlateCore",
				"CoreUObject"
			}
		);
	}
}
