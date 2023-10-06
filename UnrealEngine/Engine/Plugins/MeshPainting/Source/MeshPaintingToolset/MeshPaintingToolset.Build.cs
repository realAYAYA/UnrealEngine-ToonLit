// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MeshPaintingToolset : ModuleRules
	{
        public MeshPaintingToolset(ReadOnlyTargetRules Target) : base(Target)
		{
            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "Core",
                    "CoreUObject",
                    "Engine",
					"RenderCore",
					"RHI",
					"MeshDescription",
					"StaticMeshDescription",
					"GeometryCollectionEngine",
					"TypedElementFramework",
					"TypedElementRuntime",
					"UnrealEd",
					"InterchangeEngine",
					"InterchangePipelines",
					"Chaos"
				}
                );

                PublicDependencyModuleNames.AddRange(
                new string[] {
					"InteractiveToolsFramework",
					"EditorInteractiveToolsFramework",
					"GeometryCore",
                }
            );
        }
    }
}
