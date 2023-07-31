// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ChaosNiagara : ModuleRules
{
	public ChaosNiagara(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(new string[] {
			System.IO.Path.Combine(GetModuleDirectory("ControlRig"), "Private"),
			System.IO.Path.Combine(GetModuleDirectory("Niagara"), "Private"),
		});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Projects",
				"Slate",
				"SlateCore",
				"NiagaraCore",
				"Niagara",
				"NiagaraShader",
				"RenderCore",
				"VectorVM",
				"RHI",
				"ChaosSolverEngine",
                "Chaos",
				"PhysicsCore",
                "GeometryCollectionEngine",
				"PhysicsCore",
				"FieldSystemEngine",
			}
        );
					
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
              	"NiagaraCore",
				"Niagara",
				"NiagaraShader",
				"CoreUObject",
				"VectorVM",
				"RHI",
				"NiagaraVertexFactories",
                "ChaosSolverEngine",
                "GeometryCollectionEngine",
				"FieldSystemEngine"
            }
        );

		PrivateDefinitions.Add("CHAOS_INCLUDE_LEVEL_1=1");
	}
}
