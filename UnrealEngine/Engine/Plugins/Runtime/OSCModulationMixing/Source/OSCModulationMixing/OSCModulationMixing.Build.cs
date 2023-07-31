// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OSCModulationMixing : ModuleRules
{
	public OSCModulationMixing(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"AudioModulation",
				"OSC",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
			}
		);
	}
}
