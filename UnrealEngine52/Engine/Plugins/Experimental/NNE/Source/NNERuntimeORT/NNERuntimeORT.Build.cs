// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class NNERuntimeORT : ModuleRules
{
	public NNERuntimeORT( ReadOnlyTargetRules Target ) : base( Target )
	{
		ShortName = "NNERuntimeORT"; // Shorten to avoid path-too-long errors
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
				"Projects",
				"NNECore",
				"NNEProfiling",
			}
		);

		PrivateDependencyModuleNames.AddRange
			(
			new string[] {
				// ORT-related
				"CoreUObject",
				"ORTHelper",
				//"ONNXRuntime",
				"NNEOnnxruntimeEditor",
                "NNEUtils"
			}
		);

		// Win64-only
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDefinitions.Add("USE_DML");
			PublicDefinitions.Add("PLATFORM_WIN64");

			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"RHI",
			});

			PrivateDependencyModuleNames.Add("D3D12RHI");
			PrivateDependencyModuleNames.Add("DirectML");

			AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "DirectML");

		}
	}
}
