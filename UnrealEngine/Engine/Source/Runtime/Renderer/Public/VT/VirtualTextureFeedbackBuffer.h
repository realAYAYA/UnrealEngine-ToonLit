// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Math/IntPoint.h"
#include "Math/IntRect.h"
#include "Math/UnrealMathSSE.h"
#include "RHI.h"
#include "RenderGraphResources.h"
#include "RenderResource.h"
#include "RenderUtils.h"
#include "Templates/RefCounting.h"

class FRDGBuilder;
class FSceneViewFamily;

/** Returns the scale used to divide the scene texture resolution to get the virtual texture feedback resolution. */
extern int32 GetVirtualTextureFeedbackScale(FSceneViewFamily const* InViewFamily);

/** Get size for a virtual texture feedback buffer. This is calculated internally by applying GetVirtualTextureFeedbackScale() to the SceneTextureExtent. */
extern FIntPoint GetVirtualTextureFeedbackBufferSize(FSceneViewFamily const* InViewFamily, FIntPoint InSceneTextureExtent);

/** Returns a value from the sampling sequence used to traverse each feedback tile. */
extern uint32 SampleVirtualTextureFeedbackSequence(FSceneViewFamily const* InViewFamily, uint32 InFrameIndex);

/** 
 * Description of how to interpret an RHIBuffer that is being fed to the virtual texture feedback system.
 * For example a buffer may be a simple flat buffer, or a 2D screen-space buffer with rectangles representing multiple player viewports.
 * In the future we may also want to support append style buffers containing buffer size etc.
*/
struct FVirtualTextureFeedbackBufferDesc
{
	/** Initialize for a flat "1D" buffer. */
	RENDERER_API void Init(int32 InBufferSize);
	/** Initialize for a "2D" buffer. */
	RENDERER_API void Init2D(FIntPoint InBufferSize);
	/** Initialize for a "2D" buffer. Pass in view rectangles that define some subset of the buffer to actually analyze. */
	RENDERER_API void Init2D(FIntPoint InUnscaledBufferSize, TArrayView<FIntRect> const& InUnscaledViewRects, int32 InBufferScale);

	/** Size of buffer. 1D buffers have Y=1. */
	FIntPoint BufferSize = 0;
	/** The maximum number of rectangles to read from a 2D buffer. */
	static const uint32 MaxRectPerTransfer = 4u;
	/** The number of rectangles to read from the buffer. */
	int32 NumRects = 0;
	/** Rectangles to read from the buffer. */
	FIntRect Rects[MaxRectPerTransfer];
	/** Number of buffer elements to actually read (calculated from the Rects). */
	int32 TotalReadSize = 0;
};

/** GPU feedback buffer for tracking virtual texture requests from the GPU. */
class FVirtualTextureFeedbackBuffer : public FRenderResource
{
public:
	// Allocates and prepares the feedback buffer for submission.
	void Begin(FRDGBuilder& GraphBuilder, const FVirtualTextureFeedbackBufferDesc& Desc);

	// Submits the feedback buffer for readback.
	void End(FRDGBuilder& GraphBuilder);

	FRHIUnorderedAccessView* GetUAV() const;

	//~ Begin FRenderResource Interface
	virtual void ReleaseRHI() override;
	//~ End FRenderResource Interface

private:
	FVirtualTextureFeedbackBufferDesc Desc;
	TRefCountPtr<FRDGPooledBuffer> PooledBuffer;
	FRHIUnorderedAccessView* UAV{};
	bool bCanAccessFeedbackUAV = false;
};

/** 
 * Submit an RHIBuffer containing virtual texture feedback data to the virtual texture system.
 * The buffer is internally copied to the CPU and parsed to determine which virtual texture pages need to be mapped.
 * RHIBuffers that are passed in are expected to have been transitioned to a state for reading.
 * Multiple buffers can be transferred per frame using this function.
 * The function can be called from the render thread only.
*/
RENDERER_API void SubmitVirtualTextureFeedbackBuffer(class FRHICommandListImmediate& RHICmdList, FBufferRHIRef const& InBuffer, FVirtualTextureFeedbackBufferDesc const& InDesc);
RENDERER_API void SubmitVirtualTextureFeedbackBuffer(class FRDGBuilder& GraphBuilder, class FRDGBuffer* InBuffer, FVirtualTextureFeedbackBufferDesc const& InDesc);