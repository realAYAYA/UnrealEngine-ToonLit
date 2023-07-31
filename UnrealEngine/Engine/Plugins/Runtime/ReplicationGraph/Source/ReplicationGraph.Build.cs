// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ReplicationGraph : ModuleRules
{
	public ReplicationGraph(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"NetCore",
				"Engine",
				"EngineSettings",
				"PerfCounters"
			}
		);
	}
}
