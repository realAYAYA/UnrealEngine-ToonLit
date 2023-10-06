// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MergeActors : ModuleRules
{
	public MergeActors(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
                "ContentBrowser",
                "Documentation",
                "MeshUtilities",
                "PropertyEditor",
                "WorkspaceMenuStructure",
                "MeshReductionInterface",
                "MeshMergeUtilities",
				"GeometryProcessingInterfaces"
			}
		);

		PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "MaterialUtilities",
                "Slate",
                "SlateCore",
                "EditorFramework",
                "UnrealEd",
				"ToolWidgets"
            }
        );

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
                "ContentBrowser",
                "Documentation",
                "MeshUtilities",
                "MeshMergeUtilities",
                "MeshReductionInterface",
				"GeometryProcessingInterfaces"
            }
		);
	}
}
