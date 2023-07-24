// Copyright Epic Games, Inc. All Rights Reserved.


using UnrealBuildTool;
using System.IO;

public class NNEUtils : ModuleRules
{
	public NNEUtils(ReadOnlyTargetRules Target) : base(Target)
	{
		ShortName = "NNEUtils"; // Shorten to avoid path-too-long errors
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				Path.GetFullPath(Path.Combine(EngineDirectory, "Plugins/Experimental/NNE/Source/NNECore/Private")),
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"NNECore"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"NNEOnnxruntime",
				"NNEOnnx",
				"ORTHelper",
				}
			);


		if (Target.Platform == UnrealTargetPlatform.Win64 || 
			Target.Platform == UnrealTargetPlatform.Linux ||
			Target.Platform == UnrealTargetPlatform.Mac)
		{
			PrivateDependencyModuleNames.AddRange
				(
				new string[] {
					"Protobuf",
					"Re2" // ONNXRuntimeRE2
				}
			);
		}
	}
}

