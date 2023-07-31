// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudioMotorSimStandardComponents : ModuleRules
{
	public AudioMotorSimStandardComponents(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
		new string[]
			{
				"AudioMotorSim",
				"Core",
				"CoreUObject",
				"Engine"
			}
		);
	}
}
