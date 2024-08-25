// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class NNERuntimeORT : ModuleRules
{
	public NNERuntimeORT( ReadOnlyTargetRules Target ) : base( Target )
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange
			(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"NNE",
				"NNEOnnxruntimeEditor",
				"NNEUtilities",
				"Projects"
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"D3D12RHI",
				"DirectML",
				"RHI"
			});

			AddEngineThirdPartyPrivateStaticDependencies(Target, new string[]
			{
				"DirectML",
				"DX12"
			});

		}
	}
}
