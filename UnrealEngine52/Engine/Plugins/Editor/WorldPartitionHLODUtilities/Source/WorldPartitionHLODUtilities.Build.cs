// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class WorldPartitionHLODUtilities : ModuleRules
{
    public WorldPartitionHLODUtilities(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicDependencyModuleNames.AddRange(
            new string[]
			{
				"Core",
				"CoreUObject"
			}
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
			{
				"Engine",
				"MaterialUtilities",
				"MeshDescription",
				"MeshMergeUtilities",
				"StaticMeshDescription"
			}
        );

        PrivateIncludePathModuleNames.AddRange(
            new string[]
            {
				"GeometryProcessingInterfaces"
			}
        );

        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
                "MeshUtilities",
                "MeshReductionInterface",
				"GeometryProcessingInterfaces",
			}
        );
	}
}
