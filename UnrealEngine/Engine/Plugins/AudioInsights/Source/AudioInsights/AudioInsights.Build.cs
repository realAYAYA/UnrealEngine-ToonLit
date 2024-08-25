// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudioInsights : ModuleRules
{
	public AudioInsights(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange
		(
			new string[]
			{
				"Core",
				"TraceServices",
			}
		);	
		
		PrivateDependencyModuleNames.AddRange
		(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"AudioMixer",
				"AudioMixerCore",
				"AudioWidgets",
				"GameplayInsights",
				"InputCore",
				"LevelEditor",
				"OutputLog",
				"SignalProcessing",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"ToolWidgets",
				"TraceLog",
				"TraceAnalysis",
				"TraceInsights",
				"UnrealEd",
				"WorkspaceMenuStructure"
			}
		);		
	}
}
