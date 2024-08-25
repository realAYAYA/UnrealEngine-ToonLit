// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudioWidgets : ModuleRules
{
	public AudioWidgets(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange
		(
			new string[]
			{
				"Core",
				"UMG",
				"AudioAnalyzer",
				"AudioSynesthesia"
			}
		);

		PrivateDependencyModuleNames.AddRange
		(
			new string[]
			{
				"AudioMixer",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Slate",
				"SlateCore",
				"AdvancedWidgets",
				"SignalProcessing"
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange
			(
				new string[]
				{
					"UnrealEd",
				}
			);
		}
	}
}
