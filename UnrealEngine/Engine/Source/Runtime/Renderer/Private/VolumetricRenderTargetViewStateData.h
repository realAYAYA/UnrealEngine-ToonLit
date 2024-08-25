// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VolumetricRenderTargetViewStatedata.h
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "RendererInterface.h"
#include "RenderGraphResources.h"

class FRDGBuilder;

class FVolumetricRenderTargetViewStateData
{

public:

	FVolumetricRenderTargetViewStateData();
	~FVolumetricRenderTargetViewStateData();

	void Initialise(
		FIntPoint& ViewRectResolutionIn,
		int32 Mode,
		int32 UpsamplingMode,
		bool bCameraCut);

	void Reset();

	void PostRenderUpdate(float ViewExposure)
	{
		PreViewExposure = ViewExposure;
	}

	float GetPrevViewExposure()
	{
		return PreViewExposure;
	}

	FRDGTextureRef GetOrCreateVolumetricTracingRT(FRDGBuilder& GraphBuilder);
	FRDGTextureRef GetOrCreateVolumetricSecondaryTracingRT(FRDGBuilder& GraphBuilder);
	FRDGTextureRef GetOrCreateVolumetricTracingRTDepth(FRDGBuilder& GraphBuilder);

	FRDGTextureRef GetDstVolumetricReconstructRT(FRDGBuilder& GraphBuilder);
	FRDGTextureRef GetOrCreateDstVolumetricReconstructRT(FRDGBuilder& GraphBuilder);
	FRDGTextureRef GetOrCreateDstVolumetricReconstructRTDepth(FRDGBuilder& GraphBuilder);

	TRefCountPtr<IPooledRenderTarget> GetDstVolumetricReconstructRT();
	TRefCountPtr<IPooledRenderTarget> GetDstVolumetricReconstructRTDepth();

	FRDGTextureRef GetOrCreateSrcVolumetricReconstructRT(FRDGBuilder& GraphBuilder);
	FRDGTextureRef GetOrCreateSrcVolumetricReconstructRTDepth(FRDGBuilder& GraphBuilder);

	bool GetHistoryValid() const { return bHistoryValid; }
	const FIntPoint& GetCurrentVolumetricReconstructRTResolution() const { return VolumetricReconstructRTResolution; }
	const FIntPoint& GetCurrentVolumetricTracingRTResolution() const { return VolumetricTracingRTResolution; }
	const FIntPoint& GetCurrentTracingPixelOffset() const { return CurrentPixelOffset; }
	const uint32 GetNoiseFrameIndexModPattern() const { return NoiseFrameIndexModPattern; }

	const uint32 GetVolumetricReconstructRTDownsampleFactor() const { return VolumetricReconstructRTDownsampleFactor; }
	const uint32 GetVolumetricTracingRTDownsampleFactor() const { return VolumetricTracingRTDownsampleFactor; }

	FUintVector4 GetTracingCoordToZbufferCoordScaleBias() const;
	FUintVector4 GetTracingCoordToFullResPixelCoordScaleBias() const;

	int32 GetMode()				const { return Mode; }
	int32 GetUpsamplingMode()	const { return UpsamplingMode; }

	uint64 GetGPUSizeBytes(bool bLogSizes) const;

private:

	uint32 VolumetricReconstructRTDownsampleFactor;
	uint32 VolumetricTracingRTDownsampleFactor;

	uint32 CurrentRT;
	bool bFirstTimeUsed;
	bool bHistoryValid;
	float PreViewExposure;

	int32 FrameId;
	uint32 NoiseFrameIndex;	// This is only incremented once all Volumetric render target samples have been iterated
	uint32 NoiseFrameIndexModPattern;
	FIntPoint CurrentPixelOffset;

	FIntPoint FullResolution;
	FIntPoint VolumetricReconstructRTResolution;
	FIntPoint VolumetricTracingRTResolution;

	static constexpr uint32 kRenderTargetCount = 2;
	TRefCountPtr<IPooledRenderTarget> VolumetricReconstructRT[kRenderTargetCount];
	TRefCountPtr<IPooledRenderTarget> VolumetricReconstructRTDepth[kRenderTargetCount];

	TRefCountPtr<IPooledRenderTarget> VolumetricTracingRT;
	TRefCountPtr<IPooledRenderTarget> VolumetricSecondaryTracingRT;
	TRefCountPtr<IPooledRenderTarget> VolumetricTracingRTDepth;

	int32 Mode;
	int32 UpsamplingMode;
};


class FTemporalRenderTargetState
{

public:

	FTemporalRenderTargetState();
	~FTemporalRenderTargetState();

	void Initialise(const FIntPoint& ResolutionIn, EPixelFormat FormatIn);

	FRDGTextureRef GetOrCreateCurrentRT(FRDGBuilder& GraphBuilder);
	void ExtractCurrentRT(FRDGBuilder& GraphBuilder, FRDGTextureRef RDGRT);

	FRDGTextureRef GetOrCreatePreviousRT(FRDGBuilder& GraphBuilder);

	bool GetHistoryValid() const { return bHistoryValid; }

	bool CurrentIsValid() const { return RenderTargets[CurrentRT].IsValid(); }
	TRefCountPtr<IPooledRenderTarget> CurrentRenderTarget() const { return RenderTargets[CurrentRT]; }

	uint32 GetCurrentIndex() { return CurrentRT; }
	uint32 GetPreviousIndex() { return 1 - CurrentRT; }

	void Reset();

	uint64 GetGPUSizeBytes(bool bLogSizes) const;

private:

	uint32 CurrentRT;
	int32 FrameId;

	bool bFirstTimeUsed;
	bool bHistoryValid;

	FIntPoint Resolution;
	EPixelFormat Format;

	static constexpr uint32 kRenderTargetCount = 2;
	TRefCountPtr<IPooledRenderTarget> RenderTargets[kRenderTargetCount];
};



