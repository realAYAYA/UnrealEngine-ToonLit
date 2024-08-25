// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieGraphDataTypes.h"

#include "MovieGraphDefaultAudioRenderer.generated.h"

/**
 * Provides default audio rendering for the pipeline.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Default Audio Renderer"))
class MOVIERENDERPIPELINECORE_API UMovieGraphDefaultAudioRenderer : public UMovieGraphAudioRendererBase
{
	GENERATED_BODY()

public:
	UMovieGraphDefaultAudioRenderer() = default;

protected:
	// UMovieGraphAudioOutputBase interface
	virtual void StartAudioRecording() override;
	virtual void StopAudioRecording() override;
	virtual void ProcessAudioTick() override;
	virtual void SetupAudioRendering() override;
	virtual void TeardownAudioRendering() const override;
	// ~UMovieGraphAudioOutputBase
};
