// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class NiagaraShader : ModuleRules
{
    public NiagaraShader(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "NiagaraCore",
                "NiagaraVertexFactories",
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
            });
        }

        PublicIncludePathModuleNames.AddRange(
            new string[] {
            });

		PrivateIncludePaths.AddRange(new string[] {
			System.IO.Path.Combine(GetModuleDirectory("Renderer"), "Private"),
		});

		PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "DerivedDataCache",
                "Niagara",
            });
    }
}
