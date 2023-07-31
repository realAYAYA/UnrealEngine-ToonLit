// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ProceduralMeshComponentEditor : ModuleRules
	{
        public ProceduralMeshComponentEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Slate",
                    "SlateCore",
                    "Engine",
					"EditorFramework",
                    "UnrealEd",
                    "PropertyEditor",
                    "RenderCore",
                    "RHI",
                    "ProceduralMeshComponent",
                    "MeshDescription",
					"StaticMeshDescription",
                    "AssetTools",
                    "AssetRegistry"
                }
				);
		}
	}
}
