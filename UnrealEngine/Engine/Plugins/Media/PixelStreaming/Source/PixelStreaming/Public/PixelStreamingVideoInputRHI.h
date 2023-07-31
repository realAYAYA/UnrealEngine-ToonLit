// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PixelStreamingVideoInput.h"

/*
 * A Generic video input for RHI frames
 */
class PIXELSTREAMING_API FPixelStreamingVideoInputRHI : public FPixelStreamingVideoInput
{
public:
	FPixelStreamingVideoInputRHI() = default;
	virtual ~FPixelStreamingVideoInputRHI() = default;

protected:
	virtual TSharedPtr<FPixelCaptureCapturer> CreateCapturer(int32 FinalFormat, float FinalScale) override;
};
