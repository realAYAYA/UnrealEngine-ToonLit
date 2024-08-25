// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/FrameRate.h"
#include "AvaSequencerDisplayRate.generated.h"

USTRUCT(BlueprintType)
struct FAvaSequencerDisplayRate
{
	GENERATED_BODY()

	UPROPERTY()
	FFrameRate FrameRate;
};
