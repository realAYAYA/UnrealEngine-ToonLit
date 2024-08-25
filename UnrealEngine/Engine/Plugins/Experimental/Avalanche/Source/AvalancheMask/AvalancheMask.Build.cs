// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AvalancheMask : ModuleRules
{
	public AvalancheMask(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"AvalancheModifiers",
				"AvalancheShapes",
				"Core",
                "CoreUObject",
                "GeometryMask",
                "StructUtils",
            });

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ActorModifierCore",
				"AssetRegistry",
				"Avalanche",
				"AvalancheCore",
				"AvalancheText",
				"DeveloperSettings",
				"DynamicMaterial",
				"Engine",
				"GeometryFramework",
				"Slate",
				"SlateCore",
				"Text3D",
			});

        if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[] {
	                "AssetTools",
	                "DynamicMaterialEditor",
	                "Projects",
                    "UnrealEd",
                });
        }
    }
}
