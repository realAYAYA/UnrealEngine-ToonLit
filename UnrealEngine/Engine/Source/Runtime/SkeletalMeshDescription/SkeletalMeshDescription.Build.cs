// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SkeletalMeshDescription : ModuleRules
	{
		public SkeletalMeshDescription(ReadOnlyTargetRules Target) : base(Target)
		{
            // For GPUSkinPublicDefs.h
            PublicIncludePaths.Add("Runtime/Engine/Public");
            
            // For BoneWeights
			PublicIncludePathModuleNames.AddRange(
				new string[]
				{
					"AnimationCore"
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

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"MeshUtilitiesCommon"
				}
			);

			AddEngineThirdPartyPrivateStaticDependencies(Target, "MikkTSpace");
		}
	}
}
