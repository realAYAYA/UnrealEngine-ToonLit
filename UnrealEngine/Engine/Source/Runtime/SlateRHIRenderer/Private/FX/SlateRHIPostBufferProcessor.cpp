// Copyright Epic Games, Inc. All Rights Reserved.

#include "FX/SlateRHIPostBufferProcessor.h"

#include "RHIResources.h"
#include "SlateRHIRenderer.h"
#include "StereoRendering.h"
#include "UnrealEngine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SlateRHIPostBufferProcessor)

FTexture2DRHIRef USlateRHIPostBufferProcessor::GetBackbuffer_RenderThread(FRenderResource* InViewInfo, FRenderResource* InViewportTexture, FVector2D InElementWindowSize, FRHICommandListImmediate& InRHICmdList)
{
	FViewportInfo* ViewInfo = static_cast<FViewportInfo*>(InViewInfo);
	FSlateRenderTargetRHI* ViewportTexture = static_cast<FSlateRenderTargetRHI*>(InViewportTexture);

	bool bRenderedStereo = false;
	if (FSlateRHIRenderer::GetDrawToVRRenderTarget() == 0 && GEngine && IsValidRef(ViewInfo->GetRenderTargetTexture()) && GEngine->StereoRenderingDevice.IsValid())
	{
		GEngine->StereoRenderingDevice->RenderTexture_RenderThread(InRHICmdList, RHIGetViewportBackBuffer(ViewInfo->ViewportRHI), ViewInfo->GetRenderTargetTexture(), InElementWindowSize);
		bRenderedStereo = true;
	}

	FTexture2DRHIRef ViewportRT = bRenderedStereo ? nullptr : ViewInfo->GetRenderTargetTexture();
	FTexture2DRHIRef BackBuffer = (ViewportRT) ? ViewportRT : RHIGetViewportBackBuffer(ViewInfo->ViewportRHI);

	return BackBuffer;
}

FTexture2DRHIRef USlateRHIPostBufferProcessor::GetSrcTexture_RenderThread(FTexture2DRHIRef InBackBuffer, FRenderResource* InViewportTexture)
{
	FSlateRenderTargetRHI* ViewportTexture = static_cast<FSlateRenderTargetRHI*>(InViewportTexture);
	return GIsEditor ? ViewportTexture->GetRHIRef() : InBackBuffer;
}

FTextureReferenceRHIRef& USlateRHIPostBufferProcessor::GetDstTexture_RenderThread(UTextureRenderTarget2D* InSlatePostBuffer)
{
	return InSlatePostBuffer->TextureReference.TextureReferenceRHI;
}

FIntPoint USlateRHIPostBufferProcessor::GetDstExtent_RenderThread(FTexture2DRHIRef InBackBuffer, FRenderResource* InViewportTexture)
{
	FSlateRenderTargetRHI* ViewportTexture = static_cast<FSlateRenderTargetRHI*>(InViewportTexture);
	return GIsEditor ? FIntPoint(ViewportTexture->GetWidth(), ViewportTexture->GetHeight()) : InBackBuffer->GetDesc().Extent;
}