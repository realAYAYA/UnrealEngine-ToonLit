// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MotionTrajectory : ModuleRules
{
	public MotionTrajectory(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
                "Engine",
				"AnimGraphRuntime"
			}
		);
	}
}
