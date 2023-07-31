// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StageMonitorCommon : ModuleRules
{
	public StageMonitorCommon(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"DeveloperSettings",
				"StageDataCore"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"VPRoles",
				"VPUtilities",
			}
		);
	}
}
