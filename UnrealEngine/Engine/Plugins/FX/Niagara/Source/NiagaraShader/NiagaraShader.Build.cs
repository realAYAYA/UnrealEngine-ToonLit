// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class NiagaraShader : ModuleRules
{
    public NiagaraShader(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "CoreUObject",
                "Engine",
                "Renderer",
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "RenderCore",
                "VectorVM",
                "RHI",
                "Projects",
                "NiagaraCore",
                "NiagaraVertexFactories",
            }
        );

        if (Target.bBuildEditor == true)
        {
            PublicDependencyModuleNames.AddRange(
                new string[] {
                    "TargetPlatform",
                });

            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "DerivedDataCache",
                });
        }

        PublicIncludePathModuleNames.AddRange(
            new string[] {
            });

		PrivateIncludePaths.AddRange(new string[] {
		});

		PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "DerivedDataCache",
                "Niagara",
            });
    }
}
