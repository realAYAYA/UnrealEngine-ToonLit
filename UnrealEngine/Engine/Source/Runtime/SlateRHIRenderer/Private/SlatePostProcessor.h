// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "Layout/SlateRect.h"

class FSlatePostProcessResource;
class IRendererModule;
struct IPooledRenderTarget;

struct FPostProcessRectParams
{
	FTextureRHIRef SourceTexture;
	FSlateRect SourceRect;
	FSlateRect DestRect;
	FVector4f CornerRadius;
	FIntPoint SourceTextureSize;
	TFunction<void(FRHICommandListImmediate&, FGraphicsPipelineStateInitializer&, FRHIRenderPassInfo&)> RestoreStateFunc;
	TRefCountPtr<IPooledRenderTarget> UITarget; // not using FTextureRHIRef because we want to be able to use FRenderTargetWriteMask::Decode
	uint32 StencilRef{};
	EDisplayColorGamut HDRDisplayColorGamut;
};

struct FBlurRectParams
{
	int32 KernelSize;
	int32 DownsampleAmount;
	float Strength;
};

class FSlatePostProcessor
{
public:
	FSlatePostProcessor();
	~FSlatePostProcessor();

	void BlurRect(FRHICommandListImmediate& RHICmdList, IRendererModule& RendererModule, const FBlurRectParams& Params, const FPostProcessRectParams& RectParams);
	
	void ColorDeficiency(FRHICommandListImmediate& RHICmdList, IRendererModule& RendererModule, const FPostProcessRectParams& RectParams);
	
	void ReleaseRenderTargets();

private:
	void DownsampleRect(FRHICommandListImmediate& RHICmdList, IRendererModule& RendererModule, const FPostProcessRectParams& Params, const FIntPoint& DownsampleSize, FSlatePostProcessResource* IntermediateTargets);
	void UpsampleRect(FRHICommandListImmediate& RHICmdList, IRendererModule& RendererModule, const FPostProcessRectParams& Params, const FIntPoint& DownsampleSize, FSamplerStateRHIRef& Sampler, FSlatePostProcessResource* IntermediateTargets);
	int32 ComputeBlurWeights(int32 KernelSize, float StdDev, TArray<FVector4f>& OutWeightsAndOffsets);
private:
	TArray<FSlatePostProcessResource*> IntermediateTargetsArray;
};