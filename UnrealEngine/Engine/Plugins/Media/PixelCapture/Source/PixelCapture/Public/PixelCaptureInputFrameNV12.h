// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPixelCaptureInputFrame.h"
#include "PixelCaptureBufferNV12.h"

/**
 * A basic input frame for the Capture system that wraps a I420 buffer.
 */
class PIXELCAPTURE_API FPixelCaptureInputFrameNV12 : public IPixelCaptureInputFrame
{
public:
	FPixelCaptureInputFrameNV12(TSharedPtr<FPixelCaptureBufferNV12> Buffer);
	virtual ~FPixelCaptureInputFrameNV12() = default;

	virtual int32 GetType() const override;
	virtual int32 GetWidth() const override;
	virtual int32 GetHeight() const override;

	TSharedPtr<FPixelCaptureBufferNV12> GetBuffer() const;

private:
	TSharedPtr<FPixelCaptureBufferNV12> NV12Buffer;
};
