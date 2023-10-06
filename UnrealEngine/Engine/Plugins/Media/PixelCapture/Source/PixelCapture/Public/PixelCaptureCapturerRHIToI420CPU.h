// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PixelCaptureCapturer.h"
#include "RHI.h"

/**
 * A basic capturer that will capture RHI texture frames to I420 buffers utilizing cpu functions.
 * Involves CPU readback of GPU textures and processing of that readback data.
 * Input: FPixelCaptureInputFrameRHI
 * Output: FPixelCaptureOutputFrameI420
 */
class PIXELCAPTURE_API FPixelCaptureCapturerRHIToI420CPU : public FPixelCaptureCapturer, public TSharedFromThis<FPixelCaptureCapturerRHIToI420CPU>
{
public:
	/**
	 * Creates a new Capturer capturing the input frame at the given scale.
	 * @param InScale The scale of the resulting output capture.
	 */
	static TSharedPtr<FPixelCaptureCapturerRHIToI420CPU> Create(float InScale);
	virtual ~FPixelCaptureCapturerRHIToI420CPU();

protected:
	virtual FString GetCapturerName() const override { return "RHIToI420CPU"; }
	virtual void Initialize(int32 InputWidth, int32 InputHeight) override;
	virtual IPixelCaptureOutputFrame* CreateOutputBuffer(int32 InputWidth, int32 InputHeight) override;
	virtual void BeginProcess(const IPixelCaptureInputFrame& InputFrame, IPixelCaptureOutputFrame* OutputBuffer) override;

private:
	float Scale = 1.0f;

	FTextureRHIRef StagingTexture;
	FTextureRHIRef ReadbackTexture;
	void* ResultsBuffer = nullptr;
	int32 MappedStride = 0;

	FPixelCaptureCapturerRHIToI420CPU(float InScale);
	void OnRHIStageComplete(IPixelCaptureOutputFrame* OutputBuffer);
	void CleanUp();
};
