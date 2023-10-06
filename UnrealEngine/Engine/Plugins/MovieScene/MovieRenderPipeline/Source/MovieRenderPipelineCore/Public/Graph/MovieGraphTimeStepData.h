// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MovieGraphTimeStepData.generated.h"

class UMovieGraphEvaluatedConfig;

/** 
* This data structure needs to be filled out each frame by the UMovieGraphTimeStepBase,
* which will eventually be passed to the renderer. It controls per-sample behavior such
* as the delta time, if this is the first/last sample for an output frame, etc.
*/
USTRUCT(BlueprintType)
struct MOVIERENDERPIPELINECORE_API FMovieGraphTimeStepData
{
	GENERATED_BODY()

public:
	FMovieGraphTimeStepData()
		: OutputFrameNumber(0)
		, FrameDeltaTime(0.f)
		, WorldTimeDilation(0.f)
		, WorldSeconds(0.f)
		, MotionBlurFraction(0.f)
		, bIsFirstTemporalSampleForFrame(false)
		, bIsLastTemporalSampleForFrame(false)
		, bRequiresAccumulator(false)
	{}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Graph")
	int32 OutputFrameNumber;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Graph")
	float FrameDeltaTime;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Graph")
	float WorldTimeDilation;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Graph")
	float WorldSeconds;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Graph")
	float MotionBlurFraction;

	/** 
	* Should be set to true for the first sample of each output frame. Used to determine
	* if various systems need to reset or gather data for a new frame. Can be true at
	* the same time as bIsLastTemporalSampleForFrame (ie: 1TS)
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Graph")
	bool bIsFirstTemporalSampleForFrame;

	/**
	* Should be set to true for the last sample of each output frame. Can be true at
	* the same time as bIsFirstTemporalSampleForFrame (ie: 1TS)
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Graph")
	bool bIsLastTemporalSampleForFrame;

	/**
	* Should be set to true for every sample if there is more than one temporal sample
	* making up this render. This will cause the renderer to allocate accumulators
	* to store the multi-frame data into.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Graph")
	bool bRequiresAccumulator;

	/**
	* The evaluated config holds the configuration used for this given frame. This pointer
	* can potentially change each frame (if the graph for that frame is different) but
	* users can rely on the EvaluatedConfig being correct for a given frame, thus all
	* resolves (such as filenames) should use the config for that frame, not the latest
	* one available.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Graph")
	TObjectPtr<UMovieGraphEvaluatedConfig> EvaluatedConfig;
};
