// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MoviePipelineImagePassBase.h"
#include "MovieRenderPipelineDataTypes.h"

// Forward Declares
struct FImagePixelData;
class UMoviePipeline;

class FMoviePipelinePanoramicBlender : public MoviePipeline::IMoviePipelineOutputMerger
{
public:
	FMoviePipelinePanoramicBlender(TSharedPtr<MoviePipeline::IMoviePipelineOutputMerger> InOutputMerger, const FIntPoint InOutputResolution);
public:
	virtual FMoviePipelineMergerOutputFrame& QueueOutputFrame_GameThread(const FMoviePipelineFrameOutputState& CachedOutputState) override;
	virtual void OnCompleteRenderPassDataAvailable_AnyThread(TUniquePtr<FImagePixelData>&& InData) override;
	virtual void OnSingleSampleDataAvailable_AnyThread(TUniquePtr<FImagePixelData>&& InData) override;
	virtual void AbandonOutstandingWork() override;
	virtual int32 GetNumOutstandingFrames() const override { return PendingData.Num(); }

private:
	struct FPanoramicBlendData
	{
		double BlendStartTime;
		double BlendEndTime;
		bool bFinished;
		FIntPoint OutputBoundsMin;
		FIntPoint OutputBoundsMax;
		int32 PixelWidth;
		int32 PixelHeight;
		TArray<FLinearColor> Data;
		int32 EyeIndex;
		TSharedPtr<struct FPanoramicImagePixelDataPayload> OriginalDataPayload;
	};

	struct FPanoramicOutputFrame
	{
		// Eye Index to Blend Data. Eye Index will be -1 when not using Stereo.
		TMap<int32, TArray<TSharedPtr<FPanoramicBlendData>>> BlendedData;

		// The total number of samples we have to wait for to finish blending before being 'done'.
		int32 NumSamplesTotal;

		TArray<FLinearColor> OutputEquirectangularMap;
	};

	/** Data that is expected but not fully available yet. */
	TMap<FMoviePipelineFrameOutputState, TSharedPtr<FPanoramicOutputFrame>> PendingData;

	/** Mutex that protects adding/updating/removing from PendingData */
	FCriticalSection GlobalQueueDataMutex;
	FCriticalSection OutputDataMutex;
	FIntPoint OutputEquirectangularMapSize;

	TWeakPtr<MoviePipeline::IMoviePipelineOutputMerger> OutputMerger;
};