// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class FractureEngine : ModuleRules
	{
		public FractureEngine(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Chaos",
					"Core",
					"CoreUObject",
					"DataflowCore",
					"PlanarCut",
					"GeometryCore",
					"DynamicMesh",
					"Voronoi",
					"MeshUtilitiesCommon", // for FDisjointSet
				}
				);
		}
	}
}

