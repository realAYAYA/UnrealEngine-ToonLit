// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Text3D : ModuleRules
{
	public Text3D(ReadOnlyTargetRules Target) : base(Target)
	{
		bEnableExceptions = true;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"FreeType2",
			"GeometryCore",
			"GeometryAlgorithms",
            "HarfBuzz",
            "ICU",
            "MeshDescription",
			"SlateCore",
			"StaticMeshDescription",
		});
	}
}
