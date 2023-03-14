// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NeuralNetworkInferenceShaders : ModuleRules
{
	public NeuralNetworkInferenceShaders( ReadOnlyTargetRules Target ) : base( Target )
	{
        ShortName = "NNIShaders"; // Shorten to avoid path-too-long errors
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(ModuleDirectory, "..")
			}
		);

		PublicDependencyModuleNames.AddRange
			(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine"
			}
		);

		PrivateDependencyModuleNames.AddRange
			(
			new string[] {
				"Projects",
				"RenderCore",
				"RHI"
			}
		);
	}
}
