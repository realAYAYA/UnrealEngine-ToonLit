// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AutomationDriver : ModuleRules
{
    public AutomationDriver(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicIncludePaths.AddRange(
            new string[] {
				"Developer/AutomationDriver/Public",
            }
        );

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
