// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PoseSearch : ModuleRules
{
	public PoseSearch(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		AddEngineThirdPartyPrivateStaticDependencies(Target,
			"Eigen",
			"nanoflann"
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"AnimationCore",
				"TraceLog",
				"AnimGraphRuntime",
				"StructUtils",
				"BlendStack"
			}
		);

		if (Target.bCompileAgainstEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"AnimationModifiers",
					"AnimationBlueprintLibrary",
					"DerivedDataCache",
					"UnrealEd"
				}
			);
		}
	}
}
