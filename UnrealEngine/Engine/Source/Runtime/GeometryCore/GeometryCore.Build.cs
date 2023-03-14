// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GeometryCore : ModuleRules
{
	public GeometryCore(ReadOnlyTargetRules Target) : base(Target)
	{
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// For GPUSkinPublicDefs.h
		PublicIncludePaths.Add("Runtime/Engine/Public");

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"AnimationCore",			// For the BoneWeights.h include
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
