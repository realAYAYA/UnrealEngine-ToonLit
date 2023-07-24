// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PoseCorrectives : ModuleRules
{
	public PoseCorrectives(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
                "Engine",
				"AnimationCore",
				"AnimGraphRuntime",
				"RigVM",
				"ControlRig",
				"Eigen"
			}
			);
	}
}
