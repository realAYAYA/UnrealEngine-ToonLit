// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ChaosCloth : ModuleRules
{
    public ChaosCloth(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "ClothingSystemRuntimeCommon",
                "ClothingSystemRuntimeInterface",
                "Engine",
                "Chaos",
                "ChaosCaching"
            }
		);

        SetupModulePhysicsSupport(Target);
		PrivateDefinitions.Add("CHAOS_INCLUDE_LEVEL_1=1");
	}
}
