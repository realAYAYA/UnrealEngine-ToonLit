// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DisplayClusterTests : ModuleRules
{
	public DisplayClusterTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"DisplayCluster",
				"DisplayClusterConfiguration",
				"DisplayClusterConfigurator",
				"Core",
				"CoreUObject",
				"Engine",
				"OpenColorIO",
				"SubobjectDataInterface",
				"UnrealEd"
			});

		OptimizeCode = CodeOptimization.Never;
		PCHUsage = PCHUsageMode.NoPCHs;
	}
}
