// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VectorVM : ModuleRules
{
    public VectorVM(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject"
            }
        );

        PublicDefinitions.AddRange(
            new string[]
            {
                "VECTORVM_SUPPORTS_EXPERIMENTAL=1",
                "VECTORVM_SUPPORTS_LEGACY=1",
                "VECTORVM_SUPPORTS_SERIALIZATION=0",
                "VECTORVM_DEBUG_PRINTF=0"
            });
    }
}
