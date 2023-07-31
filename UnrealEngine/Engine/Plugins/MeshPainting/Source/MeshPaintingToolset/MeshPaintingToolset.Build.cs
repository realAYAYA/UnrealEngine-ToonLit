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
					"EditorFramework",
                    "Engine",
                    "InputCore",
                    "Slate",
                    "SlateCore",
					"RenderCore",
					"RHI",
					"Slate",
					"SlateCore",
					"MeshDescription",
					"StaticMeshDescription",
					"GeometryCollectionEngine",
					"TypedElementFramework",
					"TypedElementRuntime",
					"UnrealEd",
					"InterchangeEngine",
					"InterchangePipelines"
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