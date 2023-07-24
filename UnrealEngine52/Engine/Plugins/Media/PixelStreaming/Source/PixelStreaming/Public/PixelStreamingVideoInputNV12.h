// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PixelStreamingVideoInput.h"

/*
 * A Generic video input for NV12 frames
 */
class PIXELSTREAMING_API FPixelStreamingVideoInputNV12 : public FPixelStreamingVideoInput
{
public:
	FPixelStreamingVideoInputNV12() = default;
	virtual ~FPixelStreamingVideoInputNV12() = default;

protected:
	virtual TSharedPtr<FPixelCaptureCapturer> CreateCapturer(int32 FinalFormat, float FinalScale) override;
};
