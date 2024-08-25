// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class HarmonixMidiTests : ModuleRules
{
	public HarmonixMidiTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"HarmonixMidi",
			}
		);

		// Because we are a TEST module, we are reaching "deep" into the source code
		// of the module we are testing, which under normal circumstances one would
		// not do. 
		PrivateIncludePaths.AddRange(
			new string[] {
				Path.GetFullPath(Path.Combine(ModuleDirectory, "../HarmonixMidi/Private")),
			});
	}
}