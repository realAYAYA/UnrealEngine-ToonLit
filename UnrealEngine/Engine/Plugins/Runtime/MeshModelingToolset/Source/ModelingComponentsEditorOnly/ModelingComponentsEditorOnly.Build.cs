// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ModelingComponentsEditorOnly : ModuleRules
{
	public ModelingComponentsEditorOnly(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "InteractiveToolsFramework",
                "MeshDescription",
				"StaticMeshDescription",
				"SkeletalMeshDescription",
				"GeometryCore",
				"GeometryFramework",
				"GeometryAlgorithms",
				"DynamicMesh",
				"MeshConversion",
				"ModelingOperators",
				"ModelingComponents"
				// ... add other public dependencies that you statically link with here ...
			}
            );
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"BSPUtils",
				"CoreUObject",
				"Engine",
				"InputCore",
                "RenderCore",
				"PhysicsCore",
				"Slate",
                "RHI",
				"AssetTools",
				"UnrealEd"			// required for asset factories
			}
			);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"OpenSubdiv", // currently Win64-only
				}
				);
		}

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
