// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PixelCaptureCapturer.h"

/**
 * A basic capturer that will copy I420 frames.
 * Input: FPixelCaptureInputFrameI420
 * Output: FPixelCaptureOutputFrameI420
 */
class PIXELCAPTURE_API FPixelCaptureCapturerI420 : public FPixelCaptureCapturer
{
public:
	FPixelCaptureCapturerI420() = default;
	virtual ~FPixelCaptureCapturerI420() = default;

protected:
	virtual FString GetCapturerName() const override { return "I420 Copy"; }
	virtual void Initialize(int32 InputWidth, int32 InputHeight) override;
	virtual IPixelCaptureOutputFrame* CreateOutputBuffer(int32 InputWidth, int32 InputHeight) override;
	virtual void BeginProcess(const IPixelCaptureInputFrame& InputFrame, IPixelCaptureOutputFrame* OutputBuffer) override;
};
