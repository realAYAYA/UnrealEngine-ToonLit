// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ChaosCloth : ModuleRules
{
    public ChaosCloth(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePaths.Add("Chaos/Private");
        PublicIncludePaths.Add(ModuleDirectory + "/Public");

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
