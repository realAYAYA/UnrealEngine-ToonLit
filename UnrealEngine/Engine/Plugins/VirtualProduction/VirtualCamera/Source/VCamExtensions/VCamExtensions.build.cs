// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VCamExtensions : ModuleRules
{
	public VCamExtensions(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core", 
				"VCamCore",
				"CinematicCamera"
			}
		);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"DecoupledOutputProvider",
				"Engine",
				"InputCore",
				"Projects",
				"Slate",
				"SlateCore", 
			}
		);
	}
}
