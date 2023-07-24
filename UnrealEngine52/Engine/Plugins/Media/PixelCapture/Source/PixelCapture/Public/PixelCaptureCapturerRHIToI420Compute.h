// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PixelCaptureCapturer.h"
#include "RHI.h"

/**
 * A basic capturer that will capture RHI texture frames to I420 buffers utilizing a compute shader.
 * Involves compute shader reading and processing of GPU textures so should be faster than CPU variant.
 * Input: FPixelCaptureInputFrameRHI
 * Output: FPixelCaptureOutputFrameI420
 */
class PIXELCAPTURE_API FPixelCaptureCapturerRHIToI420Compute : public FPixelCaptureCapturer, public TSharedFromThis<FPixelCaptureCapturerRHIToI420Compute>
{
public:
	/**
	 * Creates a new Capturer capturing the input frame at the given scale.
	 * @param InScale The scale of the resulting output capture.
	 */
	static TSharedPtr<FPixelCaptureCapturerRHIToI420Compute> Create(float InScale);
	virtual ~FPixelCaptureCapturerRHIToI420Compute();

protected:
	virtual FString GetCapturerName() const override { return "RHIToI420Compute"; }
	virtual void Initialize(int32 InputWidth, int32 InputHeight) override;
	virtual IPixelCaptureOutputFrame* CreateOutputBuffer(int32 InputWidth, int32 InputHeight) override;
	virtual void BeginProcess(const IPixelCaptureInputFrame& InputFrame, IPixelCaptureOutputFrame* OutputBuffer) override;

private:
	float Scale = 1.0f;

	// dimensions of the texures
	FIntPoint PlaneYDimensions;
	FIntPoint PlaneUVDimensions;

	// used as targets for the compute shader
	FTextureRHIRef TextureY;
	FTextureRHIRef TextureU;
	FTextureRHIRef TextureV;

	// the UAVs of the targets
	FUnorderedAccessViewRHIRef TextureYUAV;
	FUnorderedAccessViewRHIRef TextureUUAV;
	FUnorderedAccessViewRHIRef TextureVUAV;

	// cpu readable copies of the targets above
	FTextureRHIRef StagingTextureY;
	FTextureRHIRef StagingTextureU;
	FTextureRHIRef StagingTextureV;

	// memory mapped pointers of the staging textures
	void* MappedY = nullptr;
	void* MappedU = nullptr;
	void* MappedV = nullptr;

	int32 YStride = 0;
	int32 UStride = 0;
	int32 VStride = 0;

	FPixelCaptureCapturerRHIToI420Compute(float InScale);
	void OnRHIStageComplete(IPixelCaptureOutputFrame* OutputBuffer);
	void CleanUp();
};
