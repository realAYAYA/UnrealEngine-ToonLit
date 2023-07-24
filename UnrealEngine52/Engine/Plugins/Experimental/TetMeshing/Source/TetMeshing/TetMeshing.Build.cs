// Copyright Epic Games, Inc. All Rights Reserved.
using System;
namespace UnrealBuildTool.Rules
{
	public class TetMeshing : ModuleRules
	{
        public TetMeshing(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.Add("TetMeshing/Private");
            PublicIncludePaths.Add(ModuleDirectory + "/Public");
            PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"MeshUtilitiesCommon",
					"StaticMeshDescription",
					"MeshConversion",
				}
				);
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
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
