// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;

public class Spatialization : ModuleRules
{
	public Spatialization(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"SignalProcessing",
				"AudioExtensions"
			}
			);

        PrivateIncludePathModuleNames.Add("TargetPlatform");
    }
}
