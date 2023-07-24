// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPixelCaptureInputFrame.h"
#include "PixelCaptureBufferI420.h"

/**
 * A basic input frame for the Capture system that wraps a I420 buffer.
 */
class PIXELCAPTURE_API FPixelCaptureInputFrameI420 : public IPixelCaptureInputFrame
{
public:
	FPixelCaptureInputFrameI420(TSharedPtr<FPixelCaptureBufferI420> Buffer);
	virtual ~FPixelCaptureInputFrameI420() = default;

	virtual int32 GetType() const override;
	virtual int32 GetWidth() const override;
	virtual int32 GetHeight() const override;

	TSharedPtr<FPixelCaptureBufferI420> GetBuffer() const;

private:
	TSharedPtr<FPixelCaptureBufferI420> I420Buffer;
};
