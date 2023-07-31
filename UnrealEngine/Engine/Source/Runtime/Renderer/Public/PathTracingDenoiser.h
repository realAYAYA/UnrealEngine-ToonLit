// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MultiGPU.h"

class FRHITexture;
class FRHICommandListImmediate;

// Spatial denoiser only Plugin
using PathTracingDenoiserFunction = void(FRHICommandListImmediate& RHICmdList, FRHITexture* ColorTex, FRHITexture* AlbedoTex, FRHITexture* NormalTex, FRHITexture* OutputTex, FRHIGPUMask GPUMask);

extern RENDERER_API PathTracingDenoiserFunction* GPathTracingDenoiserFunc;

struct FDenoisingArgumentsExt
{
	FRHITexture* FlowTex;
	FRHITexture* PreviousOutputTex;

	int Width;
	int Height;
	int DenoisingFrameId;
	bool bForceSpatialDenoiserOnly;
};

// Spatial-temporal denoiser plugin
using PathTracingSpatialTemporalDenoiserFunction = void(FRHICommandListImmediate& RHICmdList, FRHITexture* ColorTex, FRHITexture* AlbedoTex, FRHITexture* NormalTex, FRHITexture* OutputTex, const FDenoisingArgumentsExt* DenoisingArgumentExt, FRHIGPUMask GPUMask);
using PathTracingMotionVectorFunction = void(FRHICommandListImmediate& RHICmdList, FRHITexture* InputFrameTex, FRHITexture* ReferenceFrameTex, FRHITexture* FlowTex, float PreExposure, FRHIGPUMask GPUMask);

extern RENDERER_API PathTracingSpatialTemporalDenoiserFunction* GPathTracingSpatialTemporalDenoiserFunc;
extern RENDERER_API PathTracingMotionVectorFunction* GPathTracingMotionVectorFunc;