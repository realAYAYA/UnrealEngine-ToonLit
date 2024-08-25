// Copyright Epic Games, Inc. All Rights Reserved.

#include "FX/SlatePostBufferBlur.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "RHIResources.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SlatePostBufferBlur)

/////////////////////////////////////////////////////
// FSlatePostBufferBlurProxy

void FSlatePostBufferBlurProxy::PostProcess_Renderthread(FRHICommandListImmediate& RHICmdList, FRHITexture* Src, FRHITexture* Dst, FIntRect SrcRect, FIntRect DstRect, FSlateRHIRenderingPolicyInterface InRenderingPolicy)
{
	if (InRenderingPolicy.IsValid())
	{
		// Use rendering policy to perform blur post process with desired Src / Dst & respective extents
		InRenderingPolicy.BlurRectExternal(RHICmdList, Src, Dst, SrcRect, DstRect, GaussianBlurStrength_RenderThread);
	}
}

void FSlatePostBufferBlurProxy::OnUpdateValuesRenderThread()
{
	// Don't issue multiple updates in a single draw
	if (!ParamUpdateFence.IsFenceComplete())
	{
		return;
	}

	// Only issue an update when parent exists & values are different
	if (USlatePostBufferBlur* ParentBlurObject = Cast<USlatePostBufferBlur>(ParentObject))
	{
		if (ParentBlurObject->GaussianBlurStrength != GaussianBlurStrength_RenderThread)
		{
			// Explicit param copy to avoid renderthread from reading value during gamethread write
			float GaussianBlurStrengthCopy = ParentBlurObject->GaussianBlurStrength;

			// Execute param copy in a render command to safely update value on renderthread without race conditions
			TWeakPtr<FSlatePostBufferBlurProxy> TempWeakThis = SharedThis(this);
			ENQUEUE_RENDER_COMMAND(FUpdateValuesRenderThreadFX_Blur)([TempWeakThis, GaussianBlurStrengthCopy](FRHICommandListImmediate& RHICmdList)
			{
				if (TSharedPtr<FSlatePostBufferBlurProxy> SharedThisPin = TempWeakThis.Pin())
				{
					SharedThisPin->GaussianBlurStrength_RenderThread = GaussianBlurStrengthCopy;
				}
			});

			// Issue fence to prevent multiple updates in a single draw
			ParamUpdateFence.BeginFence();
		}
	}
}

/////////////////////////////////////////////////////
// USlatePostBufferBlur

USlatePostBufferBlur::USlatePostBufferBlur()
{
	RenderThreadProxy = nullptr;
}

USlatePostBufferBlur::~USlatePostBufferBlur()
{
	RenderThreadProxy = nullptr;
}

void USlatePostBufferBlur::PostProcess(FRenderResource* InViewInfo, FRenderResource* InViewportTexture, FVector2D InElementWindowSize, FSlateRHIRenderingPolicyInterface InRenderingPolicy, UTextureRenderTarget2D* InSlatePostBuffer)
{
	if (!InRenderingPolicy.IsValid())
	{
		return;
	};

	// Explicit param copy to avoid renderthread from reading value during gamethread write
	float GaussianBlurStrengthCopy = GaussianBlurStrength;

	// Enqueue default post process command, will trigger on scene before any other slate element draws
	ENQUEUE_RENDER_COMMAND(FUpdateSlatePostBuffersWithFX_Blur)([InViewInfo, InViewportTexture, InElementWindowSize, InRenderingPolicy, InSlatePostBuffer, GaussianBlurStrengthCopy](FRHICommandListImmediate& RHICmdList)
	{
		// Get Backbuffer, which can vary between PIE or standalone
		FTexture2DRHIRef BackBuffer = USlateRHIPostBufferProcessor::GetBackbuffer_RenderThread(InViewInfo, InViewportTexture, InElementWindowSize, RHICmdList);

		if (BackBuffer)
		{
			// Get Src / Dst textures & their rects, again may vary between PIE or standalone
			// Here we can simply use Src Rect since the Src texture in PIE is the 'BufferedRT' scene backbuffer without the editor
			FTexture2DRHIRef Src = USlateRHIPostBufferProcessor::GetSrcTexture_RenderThread(BackBuffer, InViewportTexture);
			FTextureReferenceRHIRef& Dst = USlateRHIPostBufferProcessor::GetDstTexture_RenderThread(InSlatePostBuffer);
			FIntPoint DstExtent = USlateRHIPostBufferProcessor::GetDstExtent_RenderThread(BackBuffer, InViewportTexture);
			FIntRect SrcRect = FIntRect(0, 0, Src->GetSizeX(), Src->GetSizeY());
			FIntRect DstRect = FIntRect(0, 0, DstExtent.X, DstExtent.Y);

			// Use rendering policy to perform blur post process with desired Src / Dst & respective extents
			InRenderingPolicy.BlurRectExternal(RHICmdList, Src, Dst, SrcRect, DstRect, GaussianBlurStrengthCopy);
		}
	});
}

TSharedPtr<FSlateRHIPostBufferProcessorProxy> USlatePostBufferBlur::GetRenderThreadProxy()
{
	if (!RenderThreadProxy && IsInGameThread())
	{
		// Create a RT proxy specific for doing blurs
		RenderThreadProxy = MakeShared<FSlatePostBufferBlurProxy>();
		RenderThreadProxy->SetOwningProcessorObject(this);
	}
	return RenderThreadProxy;
}
