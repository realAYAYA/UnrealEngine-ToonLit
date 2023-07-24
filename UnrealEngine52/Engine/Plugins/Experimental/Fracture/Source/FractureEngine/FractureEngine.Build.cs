// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace UnrealBuildTool.Rules
{
	public class FractureEngine : ModuleRules
	{
		public FractureEngine(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.Add("FractureEngine/Private");
			PublicIncludePaths.Add(ModuleDirectory + "/Public");
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Chaos",
					"Core",
					"DataflowCore",
					"PlanarCut",
					"GeometryCore",
					"DynamicMesh",
					"Voronoi"
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
				}
				);
		}
	}
}

