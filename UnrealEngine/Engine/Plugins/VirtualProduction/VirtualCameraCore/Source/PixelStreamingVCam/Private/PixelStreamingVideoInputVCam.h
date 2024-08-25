// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PixelStreamingVideoInputRHI.h"

/*
 * Use this if you want to send VCam output as video input.
 */
class PIXELSTREAMINGVCAM_API FPixelStreamingVideoInputVCam : public FPixelStreamingVideoInputRHI
{
public:
	static TSharedPtr<FPixelStreamingVideoInputVCam> Create();
	virtual ~FPixelStreamingVideoInputVCam();

	virtual FString ToString() override;

protected:
	virtual TSharedPtr<FPixelCaptureCapturer> CreateCapturer(int32 FinalFormat, float FinalScale) override;
	
private:
	FPixelStreamingVideoInputVCam();
};