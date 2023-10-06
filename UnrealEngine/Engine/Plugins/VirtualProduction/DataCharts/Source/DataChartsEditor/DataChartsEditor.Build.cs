// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DataChartsEditor : ModuleRules
{
    public DataChartsEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
				"Core",
                "CoreUObject",
				"EditorFramework",
                "Engine",
                "Projects",
                "Slate",
                "SlateCore",
                "UnrealEd",
                "PlacementMode",
            }
            );
    }
}
