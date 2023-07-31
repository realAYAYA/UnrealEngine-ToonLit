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
                "InputCore",
            }
        );

		PublicIncludePathModuleNames.AddRange(new string[] {
				"AutomationController"
		});

		if (Target.bCompileAgainstEngine && Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			DynamicallyLoadedModuleNames.AddRange(new string[] { "AutomationController" });
		}
	}
}
