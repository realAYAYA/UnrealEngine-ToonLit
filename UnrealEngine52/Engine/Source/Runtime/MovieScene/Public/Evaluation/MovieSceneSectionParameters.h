// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneSectionParameters.generated.h"

USTRUCT(BlueprintType)
struct FMovieSceneSectionParameters
{
	GENERATED_BODY()

	/** Default constructor */
	FMovieSceneSectionParameters()
		: StartFrameOffset(0)
		, EndFrameOffset(0)
		, FirstLoopStartFrameOffset(0)
		, TimeScale(1.0f)
		, HierarchicalBias(100)
		, StartOffset_DEPRECATED(0.f)
		, PrerollTime_DEPRECATED(0.0f)
		, PostrollTime_DEPRECATED(0.0f)
	{}

public:
	/** Number of frames (in display rate) to skip at the beginning of the sub-sequence. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Clipping")
	FFrameNumber StartFrameOffset;

	/** Whether this section supports looping the sub-sequence. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Clipping")
	bool bCanLoop = false;

	/** Number of frames (in display rate) to skip at the beginning of the sub-sequence. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Clipping", meta=(EditCondition="bCanLoop"))
	FFrameNumber EndFrameOffset;

	/** Number of frames (in display rate) to offset the first loop of the sub-sequence. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Clipping", meta=(EditCondition="bCanLoop"))
	FFrameNumber FirstLoopStartFrameOffset;

	/** Playback time scaling factor. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Timing")
	float TimeScale;

	/** Hierachical bias. Higher bias will take precedence. */
	UPROPERTY(config, BlueprintReadWrite, EditAnywhere, Category="Sequence")
	int32 HierarchicalBias;

	UPROPERTY()
	float StartOffset_DEPRECATED;
	UPROPERTY()
	float PrerollTime_DEPRECATED;
	UPROPERTY()
	float PostrollTime_DEPRECATED;
};
