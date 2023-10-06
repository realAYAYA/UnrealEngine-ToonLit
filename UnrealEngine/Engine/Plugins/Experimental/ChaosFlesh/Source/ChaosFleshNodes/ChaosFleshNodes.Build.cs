// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ChaosFleshNodes : ModuleRules
	{
        public ChaosFleshNodes(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"ChaosCore",
					"Chaos",
					"DataflowCore",
					"DataflowEngine",
					"ChaosFlesh",
					"ChaosFleshEngine",
					"Engine",
					"GeometryAlgorithms",
					"GeometryCore",
					"MeshConversion",
					"MeshConversionEngineTypes",
					"MeshDescription",
					"TetMeshing"
				}
			);
		}
	}
}
