// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PixelCaptureCapturer.h"

/**
 * A basic capturer that will capture I420 frames to native RHI textures.
 * Input: FPixelCaptureInputFrameI420
 * Output: FPixelCaptureOutputFrameRHI
 */
class PIXELCAPTURE_API FPixelCaptureCapturerI420ToRHI : public FPixelCaptureCapturer, public TSharedFromThis<FPixelCaptureCapturerI420ToRHI>
{
public:
	static TSharedPtr<FPixelCaptureCapturerI420ToRHI> Create();
	virtual ~FPixelCaptureCapturerI420ToRHI();

protected:
	virtual FString GetCapturerName() const override { return "I420ToRHI"; }
	virtual void Initialize(int32 InputWidth, int32 InputHeight) override;
	virtual IPixelCaptureOutputFrame* CreateOutputBuffer(int32 InputWidth, int32 InputHeight) override;
	virtual void BeginProcess(const IPixelCaptureInputFrame& InputFrame, IPixelCaptureOutputFrame* OutputBuffer) override;

private:
	FIntPoint Dimensions;
	uint8_t* ARGBBuffer = nullptr;

	FPixelCaptureCapturerI420ToRHI() = default;
};
