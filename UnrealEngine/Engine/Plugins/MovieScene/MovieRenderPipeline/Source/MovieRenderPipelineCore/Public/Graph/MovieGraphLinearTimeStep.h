// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieGraphCoreTimeStep.h"

#include "MovieGraphLinearTimeStep.generated.h"

/**
 * Advances time forward linearly until the end of the range of time that is being rendered is reached. This is useful
 * for deferred rendering (where there's a small number of temporal sub-samples and no feedback mechanism for measuring
 * noise in the final image).
 */
UCLASS(BlueprintType, meta = (DisplayName = "Linear Time Step"))
class MOVIERENDERPIPELINECORE_API UMovieGraphLinearTimeStep : public UMovieGraphCoreTimeStep
{
	GENERATED_BODY()

public:
	UMovieGraphLinearTimeStep() = default;

protected:
	virtual int32 GetNextTemporalRangeIndex() const override;
	virtual int32 GetTemporalSampleCount() const override;
};
