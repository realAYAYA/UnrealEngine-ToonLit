// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SkeletalMeshDescription : ModuleRules
	{
		public SkeletalMeshDescription(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePathModuleNames.AddRange(
				new string[]
				{
					"AnimationCore",	// For BoneWeights
					"Engine",			// For GPUSkinPublicDefs.h
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"GeometryCore",
					"MeshConversion"
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"MeshDescription",
					"StaticMeshDescription"
				}
			);

			AddEngineThirdPartyPrivateStaticDependencies(Target, "MikkTSpace");
		}
	}
}
