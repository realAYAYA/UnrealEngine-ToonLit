// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ChaosClothAsset : ModuleRules
{
	public ChaosClothAsset(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			}
		);
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Chaos"
			}
		);
	}
}
