// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPixelCaptureOutputFrame.h"
#include "PixelCaptureBufferI420.h"

/**
 * A basic output frame from the Capture system that wraps a I420 buffer.
 */
class PIXELCAPTURE_API FPixelCaptureOutputFrameI420 : public IPixelCaptureOutputFrame
{
public:
	FPixelCaptureOutputFrameI420(TSharedPtr<FPixelCaptureBufferI420> InI420Buffer)
		: I420Buffer(InI420Buffer)
	{
	}
	virtual ~FPixelCaptureOutputFrameI420() = default;

	virtual int32 GetWidth() const override { return I420Buffer->GetWidth(); }
	virtual int32 GetHeight() const override { return I420Buffer->GetHeight(); }

	TSharedPtr<FPixelCaptureBufferI420> GetI420Buffer() const { return I420Buffer; }

private:
	TSharedPtr<FPixelCaptureBufferI420> I420Buffer;
};
