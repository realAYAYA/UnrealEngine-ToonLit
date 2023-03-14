// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPixelCaptureOutputFrame.h"
#include "RHI.h"

/**
 * A basic output frame from the Capture system that wraps a RHI texture buffer.
 */
class PIXELCAPTURE_API FPixelCaptureOutputFrameRHI : public IPixelCaptureOutputFrame
{
public:
	FPixelCaptureOutputFrameRHI(FTexture2DRHIRef InFrameTexture)
		: FrameTexture(InFrameTexture)
	{
	}
	virtual ~FPixelCaptureOutputFrameRHI() = default;

	virtual int32 GetWidth() const override { return FrameTexture->GetDesc().Extent.X; }
	virtual int32 GetHeight() const override { return FrameTexture->GetDesc().Extent.Y; }

	FTexture2DRHIRef GetFrameTexture() const { return FrameTexture; }

private:
	FTexture2DRHIRef FrameTexture;
};
