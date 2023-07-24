// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FlurryEditor : ModuleRules
{
    public FlurryEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "Analytics",
                "AnalyticsVisualEditing",
                "Engine",
				"Projects",
				"DeveloperSettings"
			}
			);

		PrivateIncludePathModuleNames.AddRange(
			new string[]
            {
				"Settings"
			}
		);
	}
}
