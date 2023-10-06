// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GeometryScriptingCore : ModuleRules
{
	public GeometryScriptingCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);


		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"PhysicsCore",
				"RenderCore",
				"GeometryCore",
				"GeometryFramework",
				"DynamicMesh"
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"MeshDescription",
				"StaticMeshDescription",
				"SkeletalMeshDescription",
				"MeshConversion",
				"MeshConversionEngineTypes",
				"GeometryAlgorithms",
				"ModelingOperators",
				"ModelingComponents"
			}
			);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

		// add UnrealEd dependency if building for editor
		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.Add("EditorFramework");
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

	}
}
