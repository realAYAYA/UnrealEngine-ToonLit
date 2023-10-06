// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
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
			"GeometryAlgorithms",
			"GeometryCore",
			"HarfBuzz",
            "ICU",
            "MeshDescription",
            "Slate",
			"SlateCore",
			"StaticMeshDescription",
		});

		// Needed to reference underlying FreeType info
		// @todo: Move font vector handling to new module ("FontCore") - don't expose FT directly?
		string EnginePath = Path.GetFullPath(Target.RelativeEnginePath);
		PrivateIncludePaths.AddRange(new string[]  {
			Path.Combine(EnginePath, "Source/Runtime/SlateCore/Private")
		});
	}
}
