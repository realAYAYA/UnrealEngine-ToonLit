// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VCamBlueprintNodes : ModuleRules
{
	public VCamBlueprintNodes(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"BlueprintGraph",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Slate",
				"SlateCore",
				"VCamCore",
			}
		);
	}
}
