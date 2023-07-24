// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DisplayClusterMoviePipeline : ModuleRules
{
	public DisplayClusterMoviePipeline(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"Json",
				"DisplayCluster"
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"MovieRenderPipelineCore",
				"MovieRenderPipelineRenderPasses",
				"ImageWriteQueue",
				"RenderCore",
				"RHI",
				"ActorLayerUtilities",
				"OpenColorIO",
				"DisplayCluster",
				"DisplayClusterConfiguration",
			}
		);
	}
}
