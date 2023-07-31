// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class NeuralNetworkInferenceProfiling : ModuleRules
{
	public NeuralNetworkInferenceProfiling(ReadOnlyTargetRules Target) : base(Target)
	{

		ShortName = "NNIProfiling"; 
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
				"Engine",
			}
		);

		PrivateDependencyModuleNames.AddRange
			(
			new string[] {
				"ThirdPartyHelperAndDLLLoader",
				"Boost"
			}
		);

		PublicDefinitions.Add("WITH_NNI_STATS");
	}
}
