// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ChaosClothAssetEngine : ModuleRules
{
	public ChaosClothAssetEngine(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"CoreUObject",
				"RenderCore",
				"RHI",
				"Chaos",
				"ChaosClothAsset",
				"ChaosCloth",
				"ChaosCaching",
				"ClothingSystemRuntimeCommon",
				"ClothingSystemRuntimeInterface",
				"DataflowEngine"
			}
		);

		if (Target.bBuildEditor || Target.bCompileAgainstEditor)
		{
			PrivateDependencyModuleNames.Add("PropertyEditor");  // For adding the Cloth Component "Cloth Sim" section to the Details panel UI
		}

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"DerivedDataCache",
		});
	}
}
