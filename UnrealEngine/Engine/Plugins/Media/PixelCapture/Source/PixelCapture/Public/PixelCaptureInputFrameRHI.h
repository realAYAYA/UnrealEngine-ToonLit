// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPixelCaptureInputFrame.h"
#include "RHI.h"

/**
 * A basic input frame for the Capture system that wraps a RHI texture buffer.
 */
class PIXELCAPTURE_API FPixelCaptureInputFrameRHI : public IPixelCaptureInputFrame
{
public:
	FPixelCaptureInputFrameRHI(FTexture2DRHIRef InFrameTexture);
	virtual ~FPixelCaptureInputFrameRHI() = default;

	virtual int32 GetType() const override;
	virtual int32 GetWidth() const override;
	virtual int32 GetHeight() const override;

	FTexture2DRHIRef FrameTexture;
};
