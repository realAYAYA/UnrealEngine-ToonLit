// Copyright Epic Games, Inc. All Rights Reserved.
using System;
namespace UnrealBuildTool.Rules
{
	public class PlanarCut : ModuleRules
	{
        public PlanarCut(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.Add("PlanarCut/Private");
            PublicIncludePaths.Add(ModuleDirectory + "/Public");
            PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"MeshUtilitiesCommon",
					"StaticMeshDescription",
					"MeshConversion",
					"Eigen",
				}
				);
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Voronoi",
					"GeometryCore",
					"DynamicMesh",
					"Chaos",
					"GeometryAlgorithms",
					"MeshDescription",
                }
                );
		}
	}
}
