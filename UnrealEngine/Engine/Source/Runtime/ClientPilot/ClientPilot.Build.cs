// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ClientPilot : ModuleRules
{
    public ClientPilot(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
            }
        );

		PublicIncludePathModuleNames.AddRange(new string[] {
				"AutomationController",
				"AutomationTest",
		});

		if (Target.bCompileAgainstEngine && Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			DynamicallyLoadedModuleNames.AddRange(new string[] { "AutomationController" });
		}
	}
}
