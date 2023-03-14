// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPixelCaptureInputFrame.h"
#include "PixelCaptureI420Buffer.h"

/**
 * A basic input frame for the Capture system that wraps a I420 buffer.
 */
class PIXELCAPTURE_API FPixelCaptureInputFrameI420 : public IPixelCaptureInputFrame
{
public:
	FPixelCaptureInputFrameI420(TSharedPtr<FPixelCaptureI420Buffer> Buffer);
	virtual ~FPixelCaptureInputFrameI420() = default;

	virtual int32 GetType() const override;
	virtual int32 GetWidth() const override;
	virtual int32 GetHeight() const override;

	TSharedPtr<FPixelCaptureI420Buffer> GetBuffer() const;

private:
	TSharedPtr<FPixelCaptureI420Buffer> I420Buffer;
};
