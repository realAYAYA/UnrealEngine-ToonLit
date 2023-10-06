// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_PipelineQueue.h"

#include "Editor.h"
#include "MoviePipelineQueueSubsystem.h"

EAssetCommandResult UAssetDefinition_PipelineQueue::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	if (UMoviePipelineQueue* ValidQueue = OpenArgs.LoadFirstValid<UMoviePipelineQueue>())
	{
		UMoviePipelineQueueSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>();
		check(Subsystem);

		Subsystem->LoadQueue(ValidQueue);
	}

	return EAssetCommandResult::Handled;
}