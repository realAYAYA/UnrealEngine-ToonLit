// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DisplayClusterReplication : ModuleRules
{
	public DisplayClusterReplication(ReadOnlyTargetRules ROTargetRules) : base(ROTargetRules)
	{
		PublicDependencyModuleNames.AddRange(
		new string[] {
			"Core",
			"CoreUObject",
			"DisplayCluster",
			"DisplayClusterConfiguration",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(
		new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"NetCore",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"Sockets"
		});
	}
}
