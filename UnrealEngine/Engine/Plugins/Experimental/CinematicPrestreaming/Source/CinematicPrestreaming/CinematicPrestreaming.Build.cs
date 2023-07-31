// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CinematicPrestreaming : ModuleRules
{
	public CinematicPrestreaming(ReadOnlyTargetRules Target) : base(Target)
	{
		ShortName = "CinePrestream";

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"MovieScene"
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"RenderCore",
				"RHI"
			}
		);
	}
}
