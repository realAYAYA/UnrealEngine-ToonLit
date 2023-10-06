// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ClothingSystemRuntimeNv : ModuleRules
{
	public ClothingSystemRuntimeNv(ReadOnlyTargetRules Target) : base(Target)
	{
        SetupModulePhysicsSupport(Target);

        PublicIncludePathModuleNames.Add("ClothingSystemRuntimeInterface");

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "ClothingSystemRuntimeInterface",
                "ClothingSystemRuntimeCommon"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Engine",
            }
        );
    }
}
