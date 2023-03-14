// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VertexDeltaModel : ModuleRules
{
	public VertexDeltaModel(ReadOnlyTargetRules Target) : base(Target)
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
				"Core"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ComputeFramework",
				"CoreUObject",
				"Engine",
				"GeometryCache",
				"NeuralNetworkInference",
				"OptimusCore",
				"Projects",
				"RenderCore",
				"RHI",
				"MLDeformerFramework"
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
