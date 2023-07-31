// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NiagaraVertexFactories : ModuleRules
{
    public NiagaraVertexFactories(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[] {
				"Core",
				"Engine",		
				"RenderCore",
                "RHI",
            }
        );

		PublicIncludePathModuleNames.AddRange(
			new string[] {
                "NiagaraCore",
                "Niagara",
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[] {
				"VectorVM",
            }
        );


        PrivateIncludePaths.AddRange(
            new string[] {
            })
        ;
    }
}
