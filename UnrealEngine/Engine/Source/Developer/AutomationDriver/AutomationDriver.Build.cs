// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AutomationDriver : ModuleRules
{
    public AutomationDriver(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
				"ApplicationCore",
                "InputCore",
                "Json",
                "Slate",
                "SlateCore",
            }
        );
    }
}
