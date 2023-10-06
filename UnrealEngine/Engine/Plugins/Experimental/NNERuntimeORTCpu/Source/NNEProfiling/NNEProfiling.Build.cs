// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NNEProfiling : ModuleRules
{
	public NNEProfiling(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange
			(
			new string[] {
				"Core",
				"Engine",
			}
		);
	}
}
