// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VCamInput : ModuleRules
{
	public VCamInput(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"UMG",
				"CommonUI"
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"InputCore",
				"Projects",
				"Slate",
				"SlateCore",
				"DeveloperSettings"
			}
			);
	}
}
