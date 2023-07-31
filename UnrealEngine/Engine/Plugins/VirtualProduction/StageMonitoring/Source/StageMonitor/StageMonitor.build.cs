// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StageMonitor : ModuleRules
{
	public StageMonitor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"StageDataCore",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"Json",
				"JsonUtilities",
				"Serialization",
				"StageMonitorCommon",
				"VPRoles",
				"VPUtilities",
				
			}
		);
	}
}
