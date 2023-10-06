// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GeometryCore : ModuleRules
{
	public GeometryCore(ReadOnlyTargetRules Target) : base(Target)
	{
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"AnimationCore",			// For the BoneWeights.h include
				"Engine",					// For GPUSkinPublicDefs.h
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core"
			}
		);

		UnsafeTypeCastWarningLevel = WarningLevel.Error;
	}
}
