// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AdjustEditor : ModuleRules
{
    public AdjustEditor(ReadOnlyTargetRules Target) : base(Target)
    {
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"DeveloperSettings",
				"Analytics",
                "AnalyticsVisualEditing",
                "Engine",
				"Projects"
			}
			);
	}
}
