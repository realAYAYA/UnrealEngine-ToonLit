// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PixelCaptureCapturer.h"

/**
 * A basic capturer that will capture NV12 frames to native RHI textures.
 * Input: FPixelCaptureInputFrameNV12
 * Output: FPixelCaptureOutputFrameRHI
 */
class PIXELCAPTURE_API FPixelCaptureCapturerNV12ToRHI : public FPixelCaptureCapturer, public TSharedFromThis<FPixelCaptureCapturerNV12ToRHI>
{
public:
	static TSharedPtr<FPixelCaptureCapturerNV12ToRHI> Create();
	virtual ~FPixelCaptureCapturerNV12ToRHI();

protected:
	virtual FString GetCapturerName() const override { return "NV12ToRHI"; }
	virtual void Initialize(int32 InputWidth, int32 InputHeight) override;
	virtual IPixelCaptureOutputFrame* CreateOutputBuffer(int32 InputWidth, int32 InputHeight) override;
	virtual void BeginProcess(const IPixelCaptureInputFrame& InputFrame, IPixelCaptureOutputFrame* OutputBuffer) override;

private:
	FIntPoint Dimensions;
	uint8_t* R8Buffer = nullptr;

	FPixelCaptureCapturerNV12ToRHI() = default;
};
