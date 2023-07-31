// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PixelCaptureCapturer.h"
#include "RHI.h"

/**
 * A basic capturer that will copy RHI texture frames.
 * Input: FPixelCaptureInputFrameRHI
 * Output: FPixelCaptureOutputFrameRHI
 */
class PIXELCAPTURE_API FPixelCaptureCapturerRHI : public FPixelCaptureCapturer, public TSharedFromThis<FPixelCaptureCapturerRHI>
{
public:
	/**
	 * Creates a new Capturer capturing the input frame at the given scale.
	 * @param InScale The scale of the resulting output capture.
	 */
	static TSharedPtr<FPixelCaptureCapturerRHI> Create(float InScale);
	virtual ~FPixelCaptureCapturerRHI() = default;

protected:
	virtual FString GetCapturerName() const override { return "RHI Copy"; }
	virtual IPixelCaptureOutputFrame* CreateOutputBuffer(int32 InputWidth, int32 InputHeight) override;
	virtual void BeginProcess(const IPixelCaptureInputFrame& InputFrame, IPixelCaptureOutputFrame* OutputBuffer) override;

private:
	float Scale = 1.0f;

	FGPUFenceRHIRef Fence;

	FPixelCaptureCapturerRHI(float InScale);
	void CheckComplete();
	void OnRHIStageComplete();
};
