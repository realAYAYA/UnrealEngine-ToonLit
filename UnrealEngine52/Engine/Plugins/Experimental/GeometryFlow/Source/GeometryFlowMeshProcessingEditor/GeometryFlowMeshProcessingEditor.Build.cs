// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GeometryFlowMeshProcessingEditor : ModuleRules
{
	public GeometryFlowMeshProcessingEditor(ReadOnlyTargetRules Target) : base(Target)
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
				"GeometryCore",
				"GeometryAlgorithms",
				"DynamicMesh",
				"GeometryFlowCore",
				"GeometryFlowMeshProcessing"
			}
            );
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ModelingOperatorsEditorOnly"
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
