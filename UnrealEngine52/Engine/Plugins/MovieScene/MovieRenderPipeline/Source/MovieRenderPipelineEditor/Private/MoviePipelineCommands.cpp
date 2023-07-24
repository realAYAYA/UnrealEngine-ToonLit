// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoviePipelineCommands.h"
#include "MovieRenderPipelineStyle.h"

#define LOCTEXT_NAMESPACE "MoviePipelineCommands"

FMoviePipelineCommands::FMoviePipelineCommands()
	: TCommands<FMoviePipelineCommands>("MovieRenderPipeline", LOCTEXT("MoviePipelineCommandsLabel", "Movie Pipeline"), NAME_None, FMovieRenderPipelineStyle::StyleName)
{
}

void FMoviePipelineCommands::RegisterCommands()
{
	UI_COMMAND(ResetStatus, "ResetStatus", "Reset Status", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::R));
}

#undef LOCTEXT_NAMESPACE
