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

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject"
            });

        PrivateIncludePaths.AddRange(
            new string[] {
                "Runtime/Engine/Classes/Curves"
            });

        PublicDefinitions.AddRange(
            new string[]
            {
                "VECTORVM_SUPPORTS_EXPERIMENTAL=1",
                "VECTORVM_SUPPORTS_LEGACY=1"
            });
    }
}
