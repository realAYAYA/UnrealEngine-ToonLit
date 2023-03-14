// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NeuralNetworkInferenceQA : ModuleRules
{
	public NeuralNetworkInferenceQA( ReadOnlyTargetRules Target ) : base( Target )
	{
        ShortName = "NNIQA"; // Shorten to avoid path-too-long errors
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
				"Engine",
				"NeuralNetworkInference"
			}
		);

		PrivateDependencyModuleNames.AddRange
			(
			new string[] {
				"ModelProto",
				"ModelProtoFileReader",
				"NeuralNetworkInferenceProfiling"
			}
		);
	}
}
