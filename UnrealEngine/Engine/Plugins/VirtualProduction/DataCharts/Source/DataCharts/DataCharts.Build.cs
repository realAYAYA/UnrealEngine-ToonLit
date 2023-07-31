// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DataCharts : ModuleRules
{
    public DataCharts(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Projects",
                "Slate",
                "SlateCore",
			}
		);
    }
}
