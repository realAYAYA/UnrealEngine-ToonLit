// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoviePipelineRenderPass.h"
#include "Engine/RendererSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoviePipelineRenderPass)

void UMoviePipelineRenderPass::ValidateStateImpl()
{
	Super::ValidateStateImpl();
	if (IsAlphaInTonemapperRequired())
	{
		const URendererSettings* RenderSettings = GetDefault<URendererSettings>();
		if (RenderSettings->bEnableAlphaChannelInPostProcessing == EAlphaChannelMode::Type::Disabled)
		{
			ValidationState = EMoviePipelineValidationState::Warnings;
			ValidationResults.Add(NSLOCTEXT("MovieRenderPipeline", "Outputs_AlphaWithoutProjectSetting", "This option does not work without enabling the Alpha Support in Tonemapper setting via Project Settings > Rendering > Post Processing > Enable Alpha Channel Support."));
		}
	}
}
