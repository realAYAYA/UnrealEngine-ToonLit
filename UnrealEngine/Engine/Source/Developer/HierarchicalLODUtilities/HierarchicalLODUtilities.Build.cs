// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class HierarchicalLODUtilities : ModuleRules
{
    public HierarchicalLODUtilities(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicIncludePaths.Add("Developer/HierarchicalLODUtilities/Public");

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
				"BSPUtils",
				"EditorFramework",
				"Engine",
				"MaterialUtilities",
				"MeshDescription",
				"StaticMeshDescription",
				"UnrealEd",
                "Projects",
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
                "MeshMergeUtilities",
                "MeshReductionInterface",
				"GeometryProcessingInterfaces"
			}
        );
	}
}
