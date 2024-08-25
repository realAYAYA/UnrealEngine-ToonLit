// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HarmonixDspTests : ModuleRules
{
	public HarmonixDspTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Engine",
				"UnrealEd",
				"HarmonixDsp",
				"HarmonixMidi",
				"SignalProcessing"
			}
		);
	}
}
