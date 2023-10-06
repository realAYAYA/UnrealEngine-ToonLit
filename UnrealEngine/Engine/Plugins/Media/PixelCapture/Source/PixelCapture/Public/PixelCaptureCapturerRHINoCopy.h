// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PixelCaptureCapturer.h"
#include "RHI.h"

/**
 * A basic capturer for receiving copy-safe RHI Texture frames. This pixel capturer will shortcut the need to copy texture is the input
 * and output frame sizes match
 * 
 * Input: FPixelCaptureInputFrameRHI
 * Output: FPixelCaptureOutputFrameRHI
 */
class PIXELCAPTURE_API FPixelCaptureCapturerRHINoCopy : public FPixelCaptureCapturer, public TSharedFromThis<FPixelCaptureCapturerRHINoCopy>
{
public:
	/**
	 * Creates a new Capturer capturing the input frame at the given scale.
	 * @param InScale The scale of the resulting output capture.
	 */
	static TSharedPtr<FPixelCaptureCapturerRHINoCopy> Create(float InScale);
	virtual ~FPixelCaptureCapturerRHINoCopy() = default;

protected:
	virtual FString GetCapturerName() const override { return "RHI No Copy"; }
	virtual IPixelCaptureOutputFrame* CreateOutputBuffer(int32 InputWidth, int32 InputHeight) override;
	virtual void BeginProcess(const IPixelCaptureInputFrame& InputFrame, IPixelCaptureOutputFrame* OutputBuffer) override;

private:
	float Scale = 1.0f;

	FPixelCaptureCapturerRHINoCopy(float InScale);
};