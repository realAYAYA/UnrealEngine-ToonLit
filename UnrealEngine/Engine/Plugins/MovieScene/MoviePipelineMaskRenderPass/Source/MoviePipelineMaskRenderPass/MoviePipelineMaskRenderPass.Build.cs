// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MoviePipelineMaskRenderPass : ModuleRules
{
	public MoviePipelineMaskRenderPass(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"Json"
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"MovieRenderPipelineCore",
				"MovieRenderPipelineRenderPasses",
				"RenderCore",
                "RHI",
				"ActorLayerUtilities",
				"OpenColorIO",
			}
		);
	}
}
