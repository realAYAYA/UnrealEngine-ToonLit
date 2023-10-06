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

		// TODO: Should not be including private headers from a different module
		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(GetModuleDirectory("DisplayClusterConfigurator"), "Private"), // For DisplayClusterConfiguratorPropertyUtils.h
			});

		OptimizeCode = CodeOptimization.Never;
		PCHUsage = PCHUsageMode.NoPCHs;
	}
}
