// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ChaosClothAssetTools : ModuleRules
{
	public ChaosClothAssetTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[] {
			"MeshConversion",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Engine",
				"CoreUObject",
				"RenderCore",
				"RHI",
				"Chaos",
				"ChaosClothAsset",
				"ChaosClothAssetEngine",
				"SlateCore",
				"UnrealEd",
				"Projects",
				"ClothPainter",                 // For clothing asset exporter
				"ClothingSystemRuntimeCommon",
				"GeometryCore",					// For DynamicMesh conversion
				"SkeletalMeshDescription",		// For FSkeletalMeshAttributes::DefaultSkinWeightProfileName
			}
		);
	}
}
