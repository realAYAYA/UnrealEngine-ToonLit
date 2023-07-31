// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Water : ModuleRules
{
	public Water(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(GetModuleDirectory("Renderer"), "Private"),
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"Landmass",
				"RHI",
				"NavigationSystem",
				"AIModule",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"RenderCore",
				"Renderer",
				"NiagaraCore",
				"Niagara",
				"NiagaraShader",
				"Projects",
				"Landscape",
				"GeometryCore",
				"GeometryAlgorithms",
				"DynamicMesh",
				"ChaosCore",
				"Chaos",
				"PhysicsCore",
				"DeveloperSettings"
			}
		);

		bool bWithWaterSelectionSupport = false;
		if (Target.bBuildEditor)
        {
			bWithWaterSelectionSupport = true;

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"MeshDescription"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"SourceControl",
					"UnrealEd",
					"StaticMeshDescription",
					"MeshConversion"
				}
			);
		}
		// Add a feature define instead of relying on the generic WITH_EDITOR define
		PublicDefinitions.Add("WITH_WATER_SELECTION_SUPPORT=" + (bWithWaterSelectionSupport ? 1 : 0));
	}
}