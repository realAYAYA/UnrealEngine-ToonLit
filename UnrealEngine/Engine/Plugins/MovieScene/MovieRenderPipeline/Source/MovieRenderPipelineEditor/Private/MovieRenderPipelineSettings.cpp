// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieRenderPipelineSettings.h"
#include "MoviePipelinePIEExecutor.h"
#include "MoviePipelineNewProcessExecutor.h"
#include "MoviePipeline.h"
#include "MoviePipelineImageSequenceOutput.h"
#include "MoviePipelineDeferredPasses.h"
#include "MoviePipelineQueue.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieRenderPipelineSettings)

UMovieRenderPipelineProjectSettings::UMovieRenderPipelineProjectSettings()
{
	PresetSaveDir.Path = TEXT("/Game/Cinematics/MoviePipeline/Presets/");
	DefaultLocalExecutor = UMoviePipelinePIEExecutor::StaticClass();
	DefaultRemoteExecutor = UMoviePipelineNewProcessExecutor::StaticClass();
	DefaultExecutorJob = UMoviePipelineExecutorJob::StaticClass();
	DefaultPipeline = UMoviePipeline::StaticClass();

	DefaultClasses.Add(UMoviePipelineImageSequenceOutput_JPG::StaticClass());
	DefaultClasses.Add(UMoviePipelineDeferredPassBase::StaticClass());
}
