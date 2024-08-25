// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DataflowAssetTools : ModuleRules
{
	public DataflowAssetTools(ReadOnlyTargetRules Target) : base(Target)
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
				"DataflowCore",
				"DataflowEnginePlugin",
				"SlateCore",
				"Projects",
				"UnrealEd",
				"GeometryCore",					// For DynamicMesh conversion
				"SkeletalMeshDescription"		// For FSkeletalMeshAttributes::DefaultSkinWeightProfileName
			}
		);
	}
}
