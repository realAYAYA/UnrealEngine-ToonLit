// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NNERuntimeORTCpu : ModuleRules
{
	public NNERuntimeORTCpu( ReadOnlyTargetRules Target ) : base( Target )
	{
		bDisableStaticAnalysis = true;

		bool bIsORTSupported = (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.Mac);
		
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange
		(
			new string[] 
			{
				"Core",
				"Engine",
				"NNE",
				"NNEProfiling",
				"ORTHelper",
			}
		);

		PrivateDependencyModuleNames.AddRange
		(
			new string[] 
			{
				"CoreUObject",
				"NNEUtils",
				"NNEOnnxruntime"
			}
		);
	}
}
