// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPixelCaptureOutputFrame.h"
#include "PixelCaptureI420Buffer.h"

/**
 * A basic output frame from the Capture system that wraps a I420 buffer.
 */
class PIXELCAPTURE_API FPixelCaptureOutputFrameI420 : public IPixelCaptureOutputFrame
{
public:
	FPixelCaptureOutputFrameI420(TSharedPtr<FPixelCaptureI420Buffer> InI420Buffer)
		: I420Buffer(InI420Buffer)
	{
	}
	virtual ~FPixelCaptureOutputFrameI420() = default;

	virtual int32 GetWidth() const override { return I420Buffer->GetWidth(); }
	virtual int32 GetHeight() const override { return I420Buffer->GetHeight(); }

	TSharedPtr<FPixelCaptureI420Buffer> GetI420Buffer() const { return I420Buffer; }

private:
	TSharedPtr<FPixelCaptureI420Buffer> I420Buffer;
};
