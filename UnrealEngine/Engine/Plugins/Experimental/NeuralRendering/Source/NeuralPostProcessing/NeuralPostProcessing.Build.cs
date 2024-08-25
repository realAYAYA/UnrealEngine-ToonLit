// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NeuralPostProcessing : ModuleRules
{
	public NeuralPostProcessing(ReadOnlyTargetRules Target) : base(Target)
	{
		bTreatAsEngineModule = true;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
		
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				// ... add private dependencies that you statically link with here ...	
				"Projects",
				"RenderCore",
				"RHI",
				"Renderer",
				"NNE"
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
