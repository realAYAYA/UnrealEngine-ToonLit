// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "MovieRenderPipelineDataTypes.h"
#include "Containers/Queue.h"

// Forward Declares
struct FImagePixelData;
class UMoviePipeline;



class MOVIERENDERPIPELINECORE_API FMoviePipelineOutputMerger : public MoviePipeline::IMoviePipelineOutputMerger
{
public:
	FMoviePipelineOutputMerger(UMoviePipeline* InOwningMoviePipeline)
		: WeakMoviePipeline(MakeWeakObjectPtr(InOwningMoviePipeline))
	{
	}

public:
	virtual FMoviePipelineMergerOutputFrame& QueueOutputFrame_GameThread(const FMoviePipelineFrameOutputState& CachedOutputState) override;
	virtual void OnCompleteRenderPassDataAvailable_AnyThread(TUniquePtr<FImagePixelData>&& InData) override;
	virtual void OnSingleSampleDataAvailable_AnyThread(TUniquePtr<FImagePixelData>&& InData) override;
	virtual void AbandonOutstandingWork() override;
	virtual int32 GetNumOutstandingFrames() const override { return PendingData.Num(); }

public:
	TQueue<FMoviePipelineMergerOutputFrame> FinishedFrames;
private:
	/** The Movie Pipeline that owns us. */
	TWeakObjectPtr<UMoviePipeline> WeakMoviePipeline;

	/** Data that is expected but not fully available yet. */
	TMap<FMoviePipelineFrameOutputState, FMoviePipelineMergerOutputFrame> PendingData;

	/** Mutex that protects adding/updating/removing from ActiveData */
	FCriticalSection ActiveDataMutex;
};


